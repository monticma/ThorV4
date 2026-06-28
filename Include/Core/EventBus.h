#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <atomic>

// =============================================================================
// EventBus — File d'événements lock-free pour communication intra-processus
// =============================================================================
//
// Algorithme : Vyukov MPMC ring buffer avec numéros de séquence par slot.
// Multi-producer, single-consumer.
//
// 6 files de priorité :
//   - 1 file d'urgence (Emergency) : 2048 slots
//   - 5 files standard (Critical → Background) : 2048 slots chacune
//
// Producteurs (thread-safe, lock-free) :
//   - GalilDriver::listenLoop (thread listener port 2324)
//   - LuaEngine (main thread)
//   - Agent::tick (main thread)
//   - WebServer (threads CivetWeb, futur)
//
// Consommateur (single-thread, lock-free) :
//   - Agent::tick (main thread uniquement)
//
// Usage :
//   EventBus bus;
//   Event ev;
//   ev.type = EventType::MotionComplete;
//   ev.priority = EventPriority::Critical;
//   ev.data.motion.axisIndex = 0;
//   bus.publish(ev);
//
//   std::vector<Event> events;
//   bus.consume(32, events);
//   for (auto& e : events) { /* traiter */ }
// =============================================================================

// =============================================================================
// Types d'événements
// =============================================================================

/// @brief Catégorie d'événement.
enum class EventType : uint16_t {
    // Safety (Emergency)
    EmergencyStop         = 50,
    LimitSwitch           = 51,
    SafetyInterlock       = 52,
    AxisFault             = 53,
    WatchdogExpired        = 54,

    // Motion (Critical)
    MotionComplete        = 100,
    MotionError           = 101,
    HomeComplete          = 102,

    // Controller (Critical)
    ControllerDisconnected = 150,

    // I/O (High)
    IOChange              = 200,

    // Scripts (High/Normal)
    ScriptError           = 300,
    ScriptStarted         = 301,
    ScriptFinished        = 302,

    // Primitives (Normal)
    PrimitiveStarted      = 400,
    PrimitiveCompleted    = 401,

    // Controller (Normal)
    ControllerConnected   = 450,

    // System (Background)
    TelemetryTick         = 500,
    Shutdown              = 501
};

/// @brief Priorité d'un événement.
enum class EventPriority : uint8_t {
    Emergency  = 0,   // E-Stop, limite, axis fault — file d'urgence dédiée
    Critical   = 1,   // Motion complete, déconnexion contrôleur
    High       = 2,   // I/O, erreurs de script
    Normal     = 3,   // Primitives, connexion contrôleur
    Background = 4    // Télémétrie, shutdown
};

// =============================================================================
// Payloads typés (union dans Event)
// =============================================================================

/// @brief Payload pour les événements de motion.
struct MotionEventData {
    int     axisIndex;         // index de l'axe (0=A, 1=B, ...)
    double  position;          // position actuelle (counts)
    double  velocity;          // vitesse actuelle (counts/s)
    int     statusCode;        // 0=ok, code erreur sinon
    char    description[64];   // message lisible
};

/// @brief Payload pour les événements de sécurité.
struct SafetyEventData {
    int     errorCode;         // code d'erreur
    int     axisIndex;         // axe concerné (-1 si aucun)
    double  limitValue;        // valeur limite
    double  actualValue;       // valeur mesurée
    char    description[128];  // message lisible
};

/// @brief Payload pour les événements I/O.
struct IOEventData {
    int     pinNumber;         // numéro de la pin
    int     state;             // état actuel (0 ou 1)
    int     previousState;     // état précédent
    char    pinId[32];         // identifiant logique (ex: "chuckVacuum")
};

/// @brief Payload pour les événements de script Lua.
struct ScriptEventData {
    char    scriptName[64];    // nom du fichier script
    int     lineNumber;         // ligne de l'erreur (0 si non applicable)
    char    errorMessage[256]; // message d'erreur
};

/// @brief Payload pour les événements de primitive.
struct PrimitiveEventData {
    char    componentId[32];   // identifiant du composant (ex: "robot")
    char    primitiveName[32]; // nom de la primitive (ex: "moveTo")
    int     statusCode;        // 0=started, 1=completed, -1=failed
    double  durationMs;        // durée en ms
};

/// @brief Payload générique (fallback).
struct GenericEventData {
    int     intValue;
    double  doubleValue;
    char    stringValue[64];
};

// =============================================================================
// Event
// =============================================================================

/// @brief Événement circulant dans l'EventBus.
///
/// Contient un header fixe et un payload variable (union typée).
/// Taille totale : ~288 bytes.
struct Event {
    // --- Header (32 bytes) ---
    EventType       type;
    EventPriority   priority;
    uint8_t         generation;      // profondeur de cascade (max 8), réservé
    uint16_t        flags;           // réservé
    uint64_t        eventId;         // identifiant unique (auto-incrémenté)
    uint64_t        timestampUs;     // timestamp en microsecondes
    uint32_t        sourceModule;    // identifiant du module émetteur

    // --- Payload (union, 256 bytes max) ---
    union {
        MotionEventData     motion;
        SafetyEventData     safety;
        IOEventData         io;
        ScriptEventData     script;
        PrimitiveEventData  primitive;
        GenericEventData    generic;
        uint8_t             raw[256];
    } data;
};

// =============================================================================
// EventBus
// =============================================================================

/// @brief File d'événements lock-free multi-producer, single-consumer.
///
/// Pas de système d'abonnement : le consommateur lit tous les événements
/// et filtre selon le type. Pas de callbacks, pas de std::function.
class EventBus
{
public:
    EventBus();
    ~EventBus();

    // Non-copiable, non-déplaçable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /// @brief Publie un événement.
    ///
    /// Si la priorité est Emergency, publication dans la file d'urgence.
    /// Si la file est pleine, l'événement est perdu (drop) et le compteur
    /// dropped est incrémenté.
    ///
    /// Thread-safe : peut être appelé depuis n'importe quel thread.
    ///
    /// @return true si publié, false si drop.
    bool publish(const Event& event);

    /// @brief Consomme les événements en attente.
    ///
    /// Ordre : urgence d'abord (tous), puis Critical, High, Normal, Background.
    /// Limite le nombre d'événements par priorité pour éviter la famine
    /// des niveaux inférieurs.
    ///
    /// Doit être appelé depuis le thread principal uniquement (single consumer).
    ///
    /// @param maxPerPriority Nombre max d'événements consommés par priorité.
    /// @param eventsOut      [out] Événements dépilés (ajoutés, pas écrasés).
    /// @return Nombre total d'événements consommés.
    int consume(int maxPerPriority, std::vector<Event>& eventsOut);

    /// @brief Vérifie si la file d'urgence contient des événements.
    ///
    /// Non-bloquant. Pour un polling rapide avant consume().
    bool hasEmergencyEvents() const;

    // --- Compteurs (diagnostics) ---

    /// @brief Nombre total d'événements publiés avec succès.
    uint64_t totalPublished() const;

    /// @brief Nombre total d'événements perdus (queue pleine).
    uint64_t totalDropped() const;

    /// @brief Nombre total d'événements consommés.
    uint64_t totalConsumed() const;

private:
    // Implémentation du ring buffer (détails dans EventBus.cpp)
    class EventQueue;
    static const int kQueueSize = 2048;  // puissance de 2
    static const int kNumPriorities = 5;

    EventQueue* mEmergencyQueue;
    EventQueue* mQueues[kNumPriorities];  // Critical=0 .. Background=4

    // Compteurs globaux (relaxed atomics, précision approximative acceptable)
    std::atomic<uint64_t> mTotalPublished;
    std::atomic<uint64_t> mTotalDropped;
    std::atomic<uint64_t> mTotalConsumed;

    // Identifiant unique pour les événements
    std::atomic<uint64_t> mNextEventId;

    /// @brief Remplit les champs automatiques d'un événement avant publication.
    void prepareEvent(Event& event);
};
