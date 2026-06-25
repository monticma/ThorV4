#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>

class EventBus;

// =============================================================================
// GalilDriver — Driver TCP pour contrôleur Galil DMC
// =============================================================================
//
// Gère deux connexions TCP :
//   - Port commande (2323) : requêtes synchrones (PA, BG, TP, ...)
//   - Port message  (2324) : messages asynchrones (fins de mouvement, erreurs)
//
// Utilise GalilProtocol pour l'encodage/décodage des commandes.
// Le thread listener (port 2324) publie les événements dans l'EventBus.
//
// Usage :
//   GalilDriver driver;
//   driver.connect("192.168.1.10", 2323, 2324);
//   driver.servoHere();
//   driver.moveAbsolute({1000, 2000, 5000});
//   driver.startListener(&eventBus);
//   // ...
//   driver.stopListener();
//   driver.disconnect();
// =============================================================================

/// @brief Driver TCP pour contrôleur Galil série DMC.
class GalilDriver
{
public:
    GalilDriver();
    ~GalilDriver();

    // Non-copiable
    GalilDriver(const GalilDriver&) = delete;
    GalilDriver& operator=(const GalilDriver&) = delete;

    // =========================================================================
    // Connexion
    // =========================================================================

    /// @brief Ouvre les connexions TCP vers le contrôleur.
    /// @param address     Adresse IP ou hostname.
    /// @param commandPort Port de commande (typiquement 2323).
    /// @param messagePort Port de messages asynchrones (typiquement 2324).
    /// @return true si les deux connexions ont réussi.
    bool connect(const std::string& address, int commandPort, int messagePort);

    /// @brief Ferme les connexions TCP.
    void disconnect();

    /// @brief Vérifie si les connexions sont actives.
    bool isConnected() const;

    // =========================================================================
    // Commandes motion
    // =========================================================================

    /// @brief Mouvement absolu (PA + BG). Prend des counts par axe.
    bool moveAbsolute(const std::vector<double>& counts);

    /// @brief Mouvement relatif (PR + BG). Prend des deltas en counts.
    bool moveRelative(const std::vector<double>& deltas);

    /// @brief Démarre le mouvement sur les axes spécifiés (BG seul).
    bool beginMotion(const std::vector<int>& axisIndices);

    /// @brief Attend la fin du mouvement sur les axes spécifiés (AM).
    bool afterMotion(const std::vector<int>& axisIndices);

    /// @brief Stop immédiat de tous les axes (ST).
    bool stopAll();

    /// @brief Désactive les servos (MO).
    bool motorOff();

    /// @brief Active l'asservissement (SH).
    bool servoHere();

    /// @brief Homing d'un axe (HM).
    bool home(int axisIndex);

    /// @brief Définit les vitesses (SP). Valeurs en counts/s.
    bool setSpeeds(const std::vector<double>& speeds);

    /// @brief Définit les accélérations (AC). Valeurs en counts/s².
    bool setAccelerations(const std::vector<double>& accels);

    /// @brief Définit les décélérations (DC). Valeurs en counts/s².
    bool setDecelerations(const std::vector<double>& decels);

    // =========================================================================
    // Requêtes
    // =========================================================================

    /// @brief Récupère les positions de tous les axes (TP).
    /// @param positionsOut [out] Positions en counts.
    /// @return true si la requête a réussi.
    bool getPositions(std::vector<double>& positionsOut);

    /// @brief Récupère les positions en format compact (RP).
    bool getPositionsReport(std::vector<double>& positionsOut);

    /// @brief Lit une entrée digitale.
    /// @param pin    Numéro de pin (1-based).
    /// @param valueOut [out] true = high, false = low.
    bool getDigitalInput(int pin, bool& valueOut);

    /// @brief Active ou désactive une sortie digitale.
    /// @param pin   Numéro de pin (1-based).
    /// @param value true = set, false = clear.
    bool setDigitalOutput(int pin, bool value);

    // =========================================================================
    // Listener asynchrone (port 2324)
    // =========================================================================

    /// @brief Démarre le thread d'écoute sur le port message.
    ///
    /// Les messages non sollicités du contrôleur sont parsés et publiés
    /// dans l'EventBus sous forme d'événements.
    ///
    /// @param bus EventBus où publier les événements.
    void startListener(EventBus* bus);

    /// @brief Arrête le thread d'écoute.
    void stopListener();

    // =========================================================================
    // Diagnostic
    // =========================================================================

    /// @brief Dernier message d'erreur.
    std::string lastError() const;

    /// @brief Envoie une commande brute et retourne la réponse.
    /// Utilisé par Controller pour les commandes non couvertes par l'API standard.
    bool sendRawCommand(const std::string& command, std::string& responseOut, int timeoutMs);

private:
    int  mCommandSocket;       // socket TCP port commande (2323)
    int  mMessageSocket;       // socket TCP port message  (2324)
    bool mConnected;

    std::thread mListenerThread;
    std::atomic<bool> mListenerRunning;
    EventBus* mEventBus;       // non-owning pointer

    std::string mLastError;

    static const int mDefaultTimeoutMs = 5000;

    /// @brief Envoie une commande sur le port 2323 et lit la réponse.
    ///
    /// Ajoute \r, envoie, lit jusqu'au terminateur : ou ?.
    ///
    /// @param command    Commande Galil (sans terminateur).
    /// @param responseOut [out] Réponse brute reçue.
    /// @param timeoutMs  Timeout en ms.
    /// @return true si la communication a réussi.
    bool sendCommand(const std::string& command, std::string& responseOut, int timeoutMs);

    /// @brief Envoie une commande et vérifie une réponse OK (:).
    /// @return true si la commande a été acceptée.
    bool sendAndCheck(const std::string& command, int timeoutMs);

    /// @brief Boucle d'écoute sur le port 2324 (exécutée dans mListenerThread).
    void listenLoop();

    /// @brief Ferme un socket proprement.
    static void closeSocket(int& socket);
    
    /// @brief Configure un socket en non-bloquant pour le timeout.
    static bool setSocketTimeout(int socket, int timeoutMs);
};
