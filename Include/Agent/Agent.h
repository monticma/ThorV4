#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "Workcell.h"
#include "Core/EventBus.h"
#include "Agent/Scripting/LuaEngine.h"

// -----------------------------------------------------------------------------
// AgentConfig : lecture du fichier Config/Agent.json
// Usage:
//   AgentConfig cfg;
//   if (!cfg.loadFromFile("Config/Agent.json")) { /* erreur */ }
//   cfg.agentId ...
// -----------------------------------------------------------------------------

/// @brief Endpoint réseau (commande, télémétrie, heartbeat...).
struct NetworkEndpoint {
    std::string protocol;
    std::string address;
    int         port = 0;
    bool        bind = false;
    std::string type;
    std::string identity;   // optionnel (heartbeat)
};

/// @brief Configuration réseau de l'agent.
struct NetworkConfig {
    NetworkEndpoint command;
    NetworkEndpoint telemetry;
    NetworkEndpoint heartbeat;
    NetworkEndpoint eventRelay;
};

/// @brief Configuration d'authentification.
struct AuthConfig {
    std::string encryptionKeyPath;
    bool        hardwareBindingEnabled = false;
};

/// @brief Configuration de la base de données locale.
struct DatabaseConfig {
    std::string path;
    std::string description;
};

/// @brief Configuration de la télémétrie.
struct TelemetryConfig {
    bool                     enabled = false;
    int                      publishInterval = 0;
    std::vector<std::string> metrics;
};

/// @brief Configuration de sécurité.
struct SafetyConfig {
    bool   enableLimits             = true;
    bool   enableCollisionDetection = false;
    double maxJointError            = 1.0;
    bool   emergencyStopEnabled     = true;
    bool   softwareEStopEnabled     = true;
    int    watchdogTimeout          = 5000;
};

/// @brief Configuration de performance.
struct PerformanceConfig {
    int         controlLoopRate     = 100;
    std::string controlLoopRateUnit = "Hz";
    int         trajectoryBufferSize = 1000;
    int         maxCommandQueueSize  = 100;
};

/// @brief Configuration de simulation.
struct SimulationConfig {
    bool        enabled        = false;
    int         updateRate     = 100;
    std::string updateRateUnit = "Hz";
    std::string motionProfile  = "trapezoidal";
    bool        enablePhysics  = false;
};

/// @brief Configuration de logging.
struct LoggingConfig {
    std::string level      = "INFO";
    std::string file;
    bool        console    = true;
    int         maxSizeMB  = 50;
    int         maxBackups = 5;
};

/// @brief Entrée de pair (autre agent connecté).
struct PeerEntry {
    std::string agentId;
    std::string role;
};

/// @brief Agent Thor : lit Config/Agent.json et orchestre la WorkCell.
class Agent {
public:
    Agent();

    /// @brief Construit et charge un Agent depuis un fichier JSON.
    /// @param filePath Chemin vers le fichier Agent .json.
    Agent(const std::string& filePath);

    /// @brief Charge la configuration depuis un fichier JSON.
    /// @param filePath Chemin vers le fichier .json.
    /// @return true si le chargement a réussi.
    bool loadFromFile(const std::string& filePath);

    /// @brief Retourne le dernier message d'erreur.
    const std::string& lastError() const { return mLastError; }

    /// @brief Retourne un pointeur vers la WorkCell chargée.
    Workcell* getWorkcell() const { return workcell.get(); }

    /// @brief Retourne une référence vers l'EventBus.
    EventBus* getEventBus() { return &mEventBus; }

    /// @brief Retourne une référence vers le LuaEngine.
    LuaEngine* getLuaEngine() { return &mLuaEngine; }

    // --- Cycle de vie V4 ---

    /// @brief Initialise tous les sous-systèmes : EventBus, LuaEngine,
    ///        Workcell (loadComponents + wireComponents + startListeners).
    /// @return true si l'initialisation complète a réussi.
    bool initialize();

    /// @brief Boucle principale : consomme les événements et exécute
    ///        les ticks périodiques (télémétrie, watchdog).
    void run();

    /// @brief Arrêt gracieux : déconnecte les contrôleurs, arrête les
    ///        listeners, détruit la VM Lua.
    void shutdown();

private:
    /// @brief Traite les événements consommés depuis l'EventBus.
    void processEvents(const std::vector<Event>& events);

    /// @brief Tick périodique de télémétrie (~10 Hz).
    void telemetryTick();

public:
    // --- Champs racine ---
    std::string version;
    std::string fileType;
    std::string agentId;
    std::string name;
    std::string description;
    std::string workCellPath;

    // --- Sections ---
    NetworkConfig      network;
    AuthConfig         authentication;
    DatabaseConfig     database;
    TelemetryConfig    telemetry;
    SafetyConfig       safety;
    PerformanceConfig  performance;
    SimulationConfig   simulation;
    LoggingConfig      logging;

    std::vector<PeerEntry> peers;

    // --- Meta-données ---
    std::string              configVersion;
    std::string              created;
    std::string              modified;
    std::vector<std::string> notes;

private:
    std::unique_ptr<Workcell> workcell;
    EventBus  mEventBus;
    LuaEngine mLuaEngine;

    std::string mLastError;
    std::string mLastFilePath;
    bool mRunning = false;
};
