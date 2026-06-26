#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>

#include <nlohmann/json.hpp>

#include "Agent/Agent.h"

using json = nlohmann::json;

Agent::Agent()
{
    // Constructeur par défaut, ne charge rien
}

Agent::Agent(const std::string& filePath)
{
    loadFromFile(filePath);
}

// -----------------------------------------------------------------------------
// Charger le fichier JSON, retourne true si OK, false + lastError() sinon
// -----------------------------------------------------------------------------

/**
 * @brief Remplit un NetworkEndpoint depuis un objet JSON.
 *
 * Si la clé n'existe pas dans l'objet parent, l'endpoint reste inchangé.
 *
 * @param parent  Objet JSON contenant (ou non) la clé demandée.
 * @param key     Nom de la clé dans parent.
 * @param out     Endpoint à remplir.
 */
static void readEndpoint(const json& parent, const char* key, NetworkEndpoint& out)
{
    if (parent.contains(key)) {
        auto& e = parent[key];
        out.protocol = e.value("protocol", "");
        out.address  = e.value("address", "");
        out.port     = e.value("port", 0);
        out.bind     = e.value("bind", false);
        out.type     = e.value("type", "");
        out.identity = e.value("identity", "");
    }
}

bool Agent::loadFromFile(const std::string& filePath)
{
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        mLastError = "Cannot open file: " + filePath;
        return false;
    }

    json j;
    try {
        j = json::parse(ifs);
    } catch (const json::parse_error& e) {
        mLastError = std::string("JSON parse error: ") + e.what();
        return false;
    }

    // --- Champs racine ---
    version     = j.value("version", "");
    fileType     = j.value("fileType", "");
    agentId      = j.value("agentId", "");
    name         = j.value("name", "");
    description  = j.value("description", "");
    workCellPath = j.value("workCell", "");

    // --- network ---
    if (j.contains("network") && j["network"].contains("endpoints")) {
        auto& ep = j["network"]["endpoints"];
        readEndpoint(ep, "command",    network.command);
        readEndpoint(ep, "telemetry",  network.telemetry);
        readEndpoint(ep, "heartbeat",  network.heartbeat);
        readEndpoint(ep, "eventRelay", network.eventRelay);
    }

    // --- authentication ---
    if (j.contains("authentication")) {
        auto& a = j["authentication"];
        authentication.encryptionKeyPath      = a.value("encryptionKeyPath", "");
        authentication.hardwareBindingEnabled  = a.value("hardwareBindingEnabled", false);
    }

    // --- database ---
    if (j.contains("database")) {
        auto& d = j["database"];
        database.path        = d.value("path", "");
        database.description = d.value("description", "");
    }

    // --- telemetry ---
    if (j.contains("telemetry")) {
        auto& t = j["telemetry"];
        telemetry.enabled         = t.value("enabled", false);
        telemetry.publishInterval = t.value("publishInterval", 0);
        if (t.contains("metrics")) {
            telemetry.metrics.clear();
            for (const auto& m : t["metrics"])
                telemetry.metrics.push_back(m.get<std::string>());
        }
    }

    // --- safety ---
    if (j.contains("safety")) {
        auto& s = j["safety"];
        safety.enableLimits             = s.value("enableLimits", true);
        safety.enableCollisionDetection = s.value("enableCollisionDetection", false);
        safety.maxJointError            = s.value("maxJointError", 1.0);
        safety.emergencyStopEnabled     = s.value("emergencyStopEnabled", true);
        safety.softwareEStopEnabled     = s.value("softwareEStopEnabled", true);
        safety.watchdogTimeout          = s.value("watchdogTimeout", 5000);
    }

    // --- performance ---
    if (j.contains("performance")) {
        auto& p = j["performance"];
        performance.controlLoopRate      = p.value("controlLoopRate", 100);
        performance.controlLoopRateUnit  = p.value("controlLoopRateUnit", "Hz");
        performance.trajectoryBufferSize = p.value("trajectoryBufferSize", 1000);
        performance.maxCommandQueueSize  = p.value("maxCommandQueueSize", 100);
    }

    // --- simulation ---
    if (j.contains("simulation")) {
        auto& s = j["simulation"];
        simulation.enabled        = s.value("enabled", false);
        simulation.updateRate     = s.value("updateRate", 100);
        simulation.updateRateUnit = s.value("updateRateUnit", "Hz");
        simulation.motionProfile  = s.value("motionProfile", "trapezoidal");
        simulation.enablePhysics  = s.value("enablePhysics", false);
    }

    // --- logging ---
    if (j.contains("logging")) {
        auto& l = j["logging"];
        logging.level      = l.value("level", "INFO");
        logging.file       = l.value("file", "");
        logging.console    = l.value("console", true);
        logging.maxSizeMB  = l.value("maxSizeMB", 50);
        logging.maxBackups = l.value("maxBackups", 5);
    }

    // --- peers ---
    if (j.contains("peers") && j["peers"].contains("agents")) {
        peers.clear();
        for (const auto& a : j["peers"]["agents"]) {
            PeerEntry pe;
            pe.agentId = a.value("agentId", "");
            pe.role    = a.value("role", "");
            peers.push_back(pe);
        }
    }

    // --- metadata ---
    if (j.contains("metadata")) {
        auto& m = j["metadata"];
        configVersion = m.value("configVersion", "");
        created       = m.value("created", "");
        modified      = m.value("modified", "");
        if (m.contains("notes")) {
            notes.clear();
            for (const auto& n : m["notes"])
                notes.push_back(n.get<std::string>());
        }
    }

    // --- Charger le Workcell ---
    if (!workCellPath.empty()) {
        workcell = std::make_unique<Workcell>();
        if (!workcell->loadFromFile(workCellPath)) {
            mLastError = "Failed to load workcell: " + workcell->lastError();
            return false;
        }
    }

    mLastFilePath = filePath;
    mLastError.clear();
    return true;
}

// =============================================================================
// Cycle de vie V4
// =============================================================================

bool Agent::initialize()
{
    // 1. Initialiser le LuaEngine (sandbox Lua 5.4.7)
    if (!mLuaEngine.initialize())
    {
        mLastError = "Agent::initialize: LuaEngine failed: " + mLuaEngine.lastError();
        return false;
    }

    // 2. Donner l'EventBus au LuaEngine pour thor.publish()
    mLuaEngine.setEventBus(&mEventBus);

    // 3. Enregistrer les fonctions globales Lua (thor.log, thor.publish)
    //    déjà fait dans LuaEngine::initialize()

    // 4. Base de données : ouvrir et pré-peupler les teach points
    if (workcell)
    {
        std::string dbName = workcell->cellId;
        if (dbName.empty()) dbName = "ThorV4";
        if (!mDatabase.open(dbName))
        {
            mLastError = "Agent::initialize: Database open failed: "
                        + mDatabase.lastError();
            return false;
        }

        // Pré-peupler les teach points depuis le WorkCell.json dans la DB
        workcell->populateTeachPoints(&mDatabase);
    }

    // 5. Workcell : enregistrer les composants (et teach points) dans Lua
    if (workcell)
    {
        if (!workcell->loadComponents(mLuaEngine.state()))
        {
            mLastError = "Agent::initialize: loadComponents failed: "
                        + workcell->lastError();
            return false;
        }
    }

    // 6. Workcell : câbler les composants (Controller TCP + setController)
    if (workcell)
    {
        if (!workcell->wireComponents(&mEventBus))
        {
            mLastError = "Agent::initialize: wireComponents failed: "
                        + workcell->lastError();
            return false;
        }
    }

    // 7. Workcell : démarrer les listeners asynchrones (Galil port 2324)
    if (workcell)
    {
        workcell->startListeners();
    }

    // 8. Charger le script Lua de démarrage
    if (!mLuaEngine.loadScriptFile("Config/Scripts/startup.lua"))
    {
        // Non-bloquant : le script de démarrage est optionnel
        std::cerr << "[Agent] Warning: startup.lua not loaded: "
                  << mLuaEngine.lastError() << std::endl;
    }

    mLastError.clear();
    return true;
}

void Agent::run()
{
    // Une itération de la boucle principale.
    // Appelé depuis la boucle while() dans ThorAgent.cpp.

    // Consommer les événements de l'EventBus
    std::vector<Event> events;
    int consumed = mEventBus.consume(10, events);
    if (consumed > 0)
    {
        processEvents(events);
    }

    // Tick périodique (télémétrie, watchdog)
    telemetryTick();

    // Pause minimale pour ne pas saturer le CPU
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Agent::shutdown()
{
    mRunning = false;

    // Arrêter les listeners et déconnecter les contrôleurs
    if (workcell)
    {
        workcell->shutdown();
    }

    // LuaEngine est détruit automatiquement (pas de stop explicite)
    mLastError.clear();
}

bool Agent::executeLua(const std::string& code)
{
    if (!mLuaEngine.runString(code))
    {
        mLastError = mLuaEngine.lastError();
        return false;
    }
    return true;
}

void Agent::processEvents(const std::vector<Event>& events)
{
    for (const auto& event : events)
    {
        switch (event.type)
        {
            case EventType::ScriptStarted:
            case EventType::ScriptFinished:
                // Log les événements de script
                break;

            case EventType::ScriptError:
                std::cerr << "[Agent] Lua error: "
                          << event.data.script.errorMessage << std::endl;
                break;

            case EventType::MotionComplete:
                // Pourra déclencher des callbacks Lua plus tard
                break;

            case EventType::Shutdown:
                mRunning = false;
                break;

            default:
                break;
        }
    }
}

void Agent::telemetryTick()
{
    // Tick périodique pour la télémétrie.
    // Plus tard : publier les positions, états, compteurs...
}