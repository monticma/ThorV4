#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Workcell.h"
#include "Core/EventBus.h"

#include "Agent/Components/Aligner.h"
#include "Agent/Components/Cassette.h"
#include "Agent/Components/Controller.h"
#include "Agent/Components/CycleCounter.h"
#include "Agent/Components/EndEffector.h"
#include "Agent/Components/Flipper.h"
#include "Agent/Components/ProcessZone.h"
#include "Agent/Components/Robot.h"
#include "Agent/Components/SignalTower.h"
#include "Agent/Components/Track.h"

using json = nlohmann::json;

/**
 * @brief Fabrique un composant concret à partir de son type.
 *
 * @param type Type de composant (\"controller\", \"robot\", \"pre_aligner\", etc.).
 * @return std::unique_ptr<Component> Instance du composant, ou nullptr si type inconnu.
 */
static std::unique_ptr<Component> createComponent(const std::string& type)
{
    if (type == "controller")     return std::make_unique<Controller>();
    if (type == "robot")          return std::make_unique<Robot>();
    if (type == "pre_aligner")    return std::make_unique<Aligner>();
    if (type == "linear_track")   return std::make_unique<Track>();
    if (type == "cassette")       return std::make_unique<Cassette>();
    if (type == "flipper")        return std::make_unique<Flipper>();
    if (type == "process_zone")   return std::make_unique<ProcessZone>();
    if (type == "signal_tower")   return std::make_unique<SignalTower>();
    if (type == "cycle_counter")  return std::make_unique<CycleCounter>();
    if (type == "end_effector")   return std::make_unique<EndEffector>();
    return nullptr;
}

// -----------------------------------------------------------------------------
Workcell::Workcell(const std::string& filePath)
{
    loadFromFile(filePath);
}

Workcell::~Workcell() = default;

// -----------------------------------------------------------------------------
bool Workcell::loadFromFile(const std::string& filePath)
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
    fileType    = j.value("fileType", "");
    cellId      = j.value("cellId", "");
    cellType    = j.value("cellType", "");
    description = j.value("description", "");

    // --- units ---
    if (j.contains("units")) {
        auto& u = j["units"];
        units.length              = u.value("length", "");
        units.angle               = u.value("angle", "");
        units.linearVelocity      = u.value("linearVelocity", "");
        units.angularVelocity     = u.value("angularVelocity", "");
        units.linearAcceleration  = u.value("linearAcceleration", "");
        units.angularAcceleration = u.value("angularAcceleration", "");
        units.time                = u.value("time", "");
        units.pressure            = u.value("pressure", "");
        units.speed               = u.value("speed", "");
        units.description         = u.value("description", "");
    }

    // --- components ---
    if (j.contains("components")) {
        mComponents.clear();
        for (const auto& jc : j["components"]) {
            // Ignorer les composants inactifs
            if (!jc.value("enabled", true))
                continue;

            WorkcellComponentEntry entry;
            entry.id                   = jc.value("id", "");
            entry.type                 = jc.value("type", "");
            entry.config               = jc.value("config", "");
            entry.controller           = jc.value("controller", "");
            entry.enabled              = true;
            entry.description          = jc.value("description", "");
            entry.attachedTo           = jc.value("attachedTo", "");
            entry.mutuallyExclusiveWith = jc.value("mutuallyExclusiveWith", "");

            if (jc.contains("controllerAxes")) {
                for (const auto& a : jc["controllerAxes"])
                    entry.controllerAxes.push_back(a.get<std::string>());
            }

            // --- connection (controller uniquement) ---
            if (jc.contains("connection")) {
                auto& co = jc["connection"];
                entry.connection.protocol = co.value("protocol", "");
                entry.connection.timeout  = co.value("timeout", 5000);
                if (co.contains("channels")) {
                    for (auto& [chName, chVal] : co["channels"].items()) {
                        WorkcellConnectionChannel ch;
                        ch.address = chVal.value("address", "");
                        ch.port    = chVal.value("port", 0);
                        entry.connection.channels[chName] = ch;
                    }
                }
            }

            // --- axisMapping (controller uniquement) ---
            if (jc.contains("axisMapping")) {
                for (const auto& am : jc["axisMapping"]) {
                    AxisMappingEntry ame;
                    ame.controllerAxis = am.value("controllerAxis", "");
                    ame.component      = am.value("component", "");
                    ame.axis           = am.value("axis", "");
                    ame.role           = am.value("role", "");
                    ame.countsPerUnit  = am.value("countsPerUnit", 1.0);
                    ame.description    = am.value("description", "");
                    entry.axisMapping.push_back(ame);
                }
            }

            // --- teachPoints ---
            if (jc.contains("teachPoints")) {
                for (auto& [tpName, tpVal] : jc["teachPoints"].items()) {
                    TeachPoint tp;
                    tp.dbKey          = tpVal.value("dbKey", "");
                    tp.coordinateFrame = tpVal.value("coordinateFrame", "");
                    tp.required       = tpVal.value("required", true);
                    tp.description    = tpVal.value("description", "");
                    entry.teachPoints[tpName] = tp;
                }
            }

            // --- overrides (libre) ---
            if (jc.contains("overrides"))
                entry.overrides = jc["overrides"];

            // --- params (libre) ---
            if (jc.contains("params"))
                entry.params = jc["params"];

            // --- ioPins ---
            if (jc.contains("ioPins")) {
                for (const auto& io : jc["ioPins"]) {
                    IoPinAssignment ipa;
                    ipa.id          = io.value("id", "");
                    ipa.pin         = io.value("pin", -1);
                    ipa.description = io.value("description", "");
                    entry.ioPins.push_back(ipa);
                }
            }

            // --- position ---
            if (jc.contains("position")) {
                auto& pos = jc["position"];
                entry.position.x           = pos.value("x", 0.0);
                entry.position.y           = pos.value("y", 0.0);
                entry.position.z           = pos.value("z", 0.0);
                entry.position.theta       = pos.value("theta", 0.0);
                entry.position.description = pos.value("description", "");
            }

            // --- approach ---
            if (jc.contains("approach")) {
                auto& ap = jc["approach"];
                if (ap.contains("offset")) {
                    auto& off = ap["offset"];
                    entry.approach.offset.x = off.value("x", 0.0);
                    entry.approach.offset.y = off.value("y", 0.0);
                    entry.approach.offset.z = off.value("z", 0.0);
                }
                entry.approach.speed       = ap.value("speed", 30);
                entry.approach.description = ap.value("description", "");
            }

            // --- retract (instance) ---
            if (jc.contains("retract")) {
                auto& r = jc["retract"];
                entry.retract.strategy    = r.value("strategy", "");
                entry.retract.clearance   = r.value("clearance", 15.0);
                entry.retract.description = r.value("description", "");
            }

            // --- errorHandling ---
            if (jc.contains("errorHandling")) {
                auto& eh = jc["errorHandling"];
                entry.errorHandling.retryCount       = eh.value("retryCount", 0);
                entry.errorHandling.retryDelay       = eh.value("retryDelay", 0);
                entry.errorHandling.onFailure        = eh.value("onFailure", "");
                entry.errorHandling.retractOnError   = eh.value("retractOnError", false);
                entry.errorHandling.notifyTowerLight = eh.value("notifyTowerLight", false);
                entry.errorHandling.logLevel         = eh.value("logLevel", "");
                entry.errorHandling.description      = eh.value("description", "");
            }

            // --- speeds (instance) ---
            if (jc.contains("speeds")) {
                auto& sp = jc["speeds"];
                entry.speeds.approach    = sp.value("approach", 30);
                entry.speeds.place       = sp.value("place", 15);
                entry.speeds.pick        = sp.value("pick", 15);
                entry.speeds.description = sp.value("description", "");
            }

            // --- scanning ---
            if (jc.contains("scanning")) {
                auto& sc = jc["scanning"];
                entry.scanning.enabled         = sc.value("enabled", true);
                entry.scanning.direction       = sc.value("direction", "");
                entry.scanning.startSlot       = sc.value("startSlot", 25);
                entry.scanning.endSlot         = sc.value("endSlot", 1);
                entry.scanning.scanSpeed       = sc.value("scanSpeed", 10.0);
                entry.scanning.scanStartZ      = sc.value("scanStartZ", 0.0);
                entry.scanning.scanEndZ        = sc.value("scanEndZ", 0.0);
                entry.scanning.filterThreshold = sc.value("filterThreshold", 0.5);
                entry.scanning.description     = sc.value("description", "");
            }

            // --- scanner (endEffector) ---
            if (jc.contains("scanner")) {
                auto& sr = jc["scanner"];
                entry.scanner.enabled   = sr.value("enabled", true);
                entry.scanner.beamWidth = sr.value("beamWidth", 1.0);
                entry.scanner.description = sr.value("description", "");
                if (sr.contains("offsetFromTCP")) {
                    auto& ot = sr["offsetFromTCP"];
                    entry.scanner.offsetFromTCP.x = ot.value("x", 0.0);
                    entry.scanner.offsetFromTCP.y = ot.value("y", 0.0);
                    entry.scanner.offsetFromTCP.z = ot.value("z", 0.0);
                }
                if (sr.contains("ioPins")) {
                    for (const auto& io : sr["ioPins"]) {
                        IoPinAssignment ipa;
                        ipa.id          = io.value("id", "");
                        ipa.pin         = io.value("pin", -1);
                        ipa.description = io.value("description", "");
                        entry.scanner.ioPins.push_back(ipa);
                    }
                }
            }

            // Instancier le composant concret et charger sa config
            if (!entry.config.empty()) {
                entry.instance = createComponent(entry.type);
                if (entry.instance) {
                    if (!entry.instance->loadFromFile(entry.config)) {
                        mLastError = "Failed to load component '" + entry.id
                                    + "' from " + entry.config + ": "
                                    + entry.instance->lastError();
                        return false;
                    }
                }
            }

            mComponents.push_back(std::move(entry));
        }
    }

    // --- workflow ---
    if (j.contains("workflow")) {
        auto& wf = j["workflow"];
        workflow.defaultSequence         = wf.value("defaultSequence", "");
        workflow.processingOrder         = wf.value("processingOrder", "");
        workflow.scanBeforeEachCassette  = wf.value("scanBeforeEachCassette", true);
        workflow.alignBeforeProcess      = wf.value("alignBeforeProcess", true);
        workflow.returnAfterProcess      = wf.value("returnAfterProcess", true);
        workflow.returnToSameSlot        = wf.value("returnToSameSlot", true);
        workflow.pauseOnCassetteExhausted = wf.value("pauseOnCassetteExhausted", true);
        workflow.autoResumeOnNewCassette  = wf.value("autoResumeOnNewCassette", true);

        if (wf.contains("operatorInterface")) {
            auto& oi = wf["operatorInterface"];
            workflow.operatorInterface.resumeButtonPin         = oi.value("resumeButtonPin", 0);
            workflow.operatorInterface.resumeButtonController  = oi.value("resumeButtonController", "");
            workflow.operatorInterface.description             = oi.value("description", "");
        }

        if (wf.contains("speeds")) {
            auto& s = wf["speeds"];
            workflow.speeds.transit        = s.value("transit", 100);
            workflow.speeds.approach       = s.value("approach", 30);
            workflow.speeds.insertCassette = s.value("insertCassette", 15);
            workflow.speeds.scan           = s.value("scan", 10);
            workflow.speeds.description    = s.value("description", "");
        }

        if (wf.contains("retract")) {
            auto& r = wf["retract"];
            workflow.retract.defaultStrategy       = r.value("defaultStrategy", "");
            workflow.retract.defaultClearance       = r.value("defaultClearance", 15.0);
            workflow.retract.retractBeforeRotation  = r.value("retractBeforeRotation", true);
            workflow.retract.description            = r.value("description", "");
        }

        if (wf.contains("timings")) {
            auto& t = wf["timings"];
            workflow.timings.vacuumSettle       = t.value("vacuumSettle", 200);
            workflow.timings.placeDwell          = t.value("placeDwell", 100);
            workflow.timings.pickDwell           = t.value("pickDwell", 100);
            workflow.timings.cassetteSwapTimeout = t.value("cassetteSwapTimeout", 120000);
            workflow.timings.description         = t.value("description", "");
        }
    }

    // --- workspace ---
    if (j.contains("workspace")) {
        auto& ws = j["workspace"];

        if (ws.contains("cellBoundary")) {
            auto& cb = ws["cellBoundary"];
            workspace.cellBoundary.min.x       = cb["min"].value("x", 0.0);
            workspace.cellBoundary.min.y       = cb["min"].value("y", 0.0);
            workspace.cellBoundary.min.z       = cb["min"].value("z", 0.0);
            workspace.cellBoundary.max.x       = cb["max"].value("x", 0.0);
            workspace.cellBoundary.max.y       = cb["max"].value("y", 0.0);
            workspace.cellBoundary.max.z       = cb["max"].value("z", 0.0);
            workspace.cellBoundary.description = cb.value("description", "");
        }

        if (ws.contains("exclusionZones")) {
            workspace.exclusionZones.clear();
            for (const auto& ez : ws["exclusionZones"]) {
                WorkcellExclusionZone zone;
                zone.name        = ez.value("name", "");
                zone.type        = ez.value("type", "");
                zone.description = ez.value("description", "");
                if (ez.contains("center")) {
                    zone.center.x = ez["center"].value("x", 0.0);
                    zone.center.y = ez["center"].value("y", 0.0);
                    zone.center.z = ez["center"].value("z", 0.0);
                }
                zone.radius = ez.value("radius", 0.0);
                zone.height = ez.value("height", 0.0);
                if (ez.contains("min")) {
                    zone.min.x = ez["min"].value("x", 0.0);
                    zone.min.y = ez["min"].value("y", 0.0);
                    zone.min.z = ez["min"].value("z", 0.0);
                }
                if (ez.contains("max")) {
                    zone.max.x = ez["max"].value("x", 0.0);
                    zone.max.y = ez["max"].value("y", 0.0);
                    zone.max.z = ez["max"].value("z", 0.0);
                }
                workspace.exclusionZones.push_back(zone);
            }
        }
    }

    // --- metadata ---
    if (j.contains("metadata")) {
        auto& m = j["metadata"];
        metadata.configVersion = m.value("configVersion", "");
        metadata.created       = m.value("created", "");
        metadata.modified      = m.value("modified", "");
        if (m.contains("notes")) {
            metadata.notes.clear();
            for (const auto& n : m["notes"])
                metadata.notes.push_back(n.get<std::string>());
        }
    }

    mLastError.clear();
    return true;
}

// -----------------------------------------------------------------------------
Component* Workcell::findComponent(const std::string& id) const
{
    for (const auto& c : mComponents) {
        if (c.id == id)
            return c.instance.get();
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
void Workcell::listComponents() const
{
    std::cout << "========================================\n";
    std::cout << "Workcell : " << cellId << " (" << cellType << ")\n";
    std::cout << "Description : " << description << "\n";
    std::cout << "Version : " << version << "\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Composants (" << mComponents.size() << ") :\n";

    for (const auto& c : mComponents) {
        std::cout << "  [" << (c.enabled ? "ACTIF" : "off  ") << "] "
                  << c.id << " (" << c.type << ")";

        if (c.instance) {
            std::cout << " v" << c.instance->version;
        }

        if (!c.controller.empty())
            std::cout << "  -> ctrl: " << c.controller;

        std::cout << "\n";

        if (!c.description.empty())
            std::cout << "    " << c.description << "\n";
    }

    std::cout << "========================================\n";
}

// -----------------------------------------------------------------------------
// Helpers statiques pour le dump Workcell
// -----------------------------------------------------------------------------

namespace {

void wdumpStr(const std::string& key, const std::string& value, int indent = 2)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << value << "\n";
}

void wdumpInt(const std::string& key, int value, int indent = 2)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << value << "\n";
}

void wdumpDouble(const std::string& key, double value, int indent = 2)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << value << "\n";
}

void wdumpBool(const std::string& key, bool value, int indent = 2)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << (value ? "true" : "false") << "\n";
}

void wdumpStrVec(const std::string& key, const std::vector<std::string>& vec,
                 int indent = 4)
{
    if (vec.empty()) return;
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : [";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << vec[i];
    }
    std::cout << "]\n";
}

void wdumpJson(const std::string& key, const nlohmann::json& j, int indent = 4)
{
    if (j.is_null()) return;
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << key << " : " << j.dump() << "\n";
}

void wdumpIoPinEntry(const IoPinAssignment& io, int indent = 8)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << "- id=" << io.id << " pin=" << io.pin;
    if (!io.description.empty())
        std::cout << " (" << io.description << ")";
    std::cout << "\n";
}

void wdumpTeachPoint(const std::string& name, const TeachPoint& tp, int indent = 8)
{
    for (int i = 0; i < indent; ++i) std::cout << " ";
    std::cout << "- " << name
              << " dbKey=" << tp.dbKey
              << " frame=" << tp.coordinateFrame
              << " required=" << (tp.required ? "true" : "false");
    if (!tp.description.empty())
        std::cout << " (" << tp.description << ")";
    std::cout << "\n";
}

} // namespace

// -----------------------------------------------------------------------------

void Workcell::dump() const
{
    using std::cout;

    cout << "========================================\n";
    cout << "WORKCELL DUMP\n";
    cout << "========================================\n";

    // --- Champs racine ---
    cout << "[Root]\n";
    wdumpStr("version", version);
    wdumpStr("fileType", fileType);
    wdumpStr("cellId", cellId);
    wdumpStr("cellType", cellType);
    wdumpStr("description", description);

    // --- units ---
    cout << "[Units]\n";
    wdumpStr("length", units.length);
    wdumpStr("angle", units.angle);
    wdumpStr("linearVelocity", units.linearVelocity);
    wdumpStr("angularVelocity", units.angularVelocity);
    wdumpStr("linearAcceleration", units.linearAcceleration);
    wdumpStr("angularAcceleration", units.angularAcceleration);
    wdumpStr("time", units.time);
    wdumpStr("pressure", units.pressure);
    wdumpStr("speed", units.speed);
    wdumpStr("description", units.description);

    // --- workflow ---
    cout << "[Workflow]\n";
    wdumpStr("defaultSequence", workflow.defaultSequence);
    wdumpStr("processingOrder", workflow.processingOrder);
    wdumpBool("scanBeforeEachCassette", workflow.scanBeforeEachCassette);
    wdumpBool("alignBeforeProcess", workflow.alignBeforeProcess);
    wdumpBool("returnAfterProcess", workflow.returnAfterProcess);
    wdumpBool("returnToSameSlot", workflow.returnToSameSlot);
    wdumpBool("pauseOnCassetteExhausted", workflow.pauseOnCassetteExhausted);
    wdumpBool("autoResumeOnNewCassette", workflow.autoResumeOnNewCassette);

    cout << "  operatorInterface:\n";
    wdumpInt("resumeButtonPin", workflow.operatorInterface.resumeButtonPin, 4);
    wdumpStr("resumeButtonController", workflow.operatorInterface.resumeButtonController, 4);
    wdumpStr("description", workflow.operatorInterface.description, 4);

    cout << "  speeds:\n";
    wdumpInt("transit", workflow.speeds.transit, 4);
    wdumpInt("approach", workflow.speeds.approach, 4);
    wdumpInt("insertCassette", workflow.speeds.insertCassette, 4);
    wdumpInt("scan", workflow.speeds.scan, 4);
    wdumpStr("description", workflow.speeds.description, 4);

    cout << "  retract:\n";
    wdumpStr("defaultStrategy", workflow.retract.defaultStrategy, 4);
    wdumpDouble("defaultClearance", workflow.retract.defaultClearance, 4);
    wdumpBool("retractBeforeRotation", workflow.retract.retractBeforeRotation, 4);
    wdumpStr("description", workflow.retract.description, 4);

    cout << "  timings:\n";
    wdumpInt("vacuumSettle", workflow.timings.vacuumSettle, 4);
    wdumpInt("placeDwell", workflow.timings.placeDwell, 4);
    wdumpInt("pickDwell", workflow.timings.pickDwell, 4);
    wdumpInt("cassetteSwapTimeout", workflow.timings.cassetteSwapTimeout, 4);
    wdumpStr("description", workflow.timings.description, 4);

    // --- workspace ---
    cout << "[Workspace]\n";
    cout << "  cellBoundary:\n";
    cout << "    min: x=" << workspace.cellBoundary.min.x
              << " y=" << workspace.cellBoundary.min.y
              << " z=" << workspace.cellBoundary.min.z << "\n";
    cout << "    max: x=" << workspace.cellBoundary.max.x
              << " y=" << workspace.cellBoundary.max.y
              << " z=" << workspace.cellBoundary.max.z << "\n";
    wdumpStr("description", workspace.cellBoundary.description, 4);

    if (!workspace.exclusionZones.empty()) {
        cout << "  exclusionZones (" << workspace.exclusionZones.size() << ") :\n";
        for (const auto& ez : workspace.exclusionZones) {
            cout << "    - name=" << ez.name
                      << " type=" << ez.type;
            if (ez.type == "cylinder") {
                cout << " center=(" << ez.center.x
                          << "," << ez.center.y
                          << "," << ez.center.z << ")"
                          << " radius=" << ez.radius
                          << " height=" << ez.height;
            } else {
                cout << " min=(" << ez.min.x
                          << "," << ez.min.y
                          << "," << ez.min.z << ")"
                          << " max=(" << ez.max.x
                          << "," << ez.max.y
                          << "," << ez.max.z << ")";
            }
            if (!ez.description.empty())
                cout << " (" << ez.description << ")";
            cout << "\n";
        }
    }

    // --- metadata ---
    cout << "[Metadata]\n";
    wdumpStr("configVersion", metadata.configVersion);
    wdumpStr("created", metadata.created);
    wdumpStr("modified", metadata.modified);
    wdumpStrVec("notes", metadata.notes, 2);

    // --- Components ---
    cout << "========================================\n";
    cout << "[Components] (" << mComponents.size() << " entries)\n";
    cout << "========================================\n";

    for (size_t idx = 0; idx < mComponents.size(); ++idx) {
        const auto& c = mComponents[idx];
        cout << "----------------------------------------\n";
        cout << "Component #" << idx << " : " << c.id << "\n";
        cout << "----------------------------------------\n";

        wdumpStr("id", c.id);
        wdumpStr("type", c.type);
        wdumpStr("config", c.config);
        wdumpStr("controller", c.controller);
        wdumpStrVec("controllerAxes", c.controllerAxes);
        wdumpBool("enabled", c.enabled);
        wdumpStr("description", c.description);
        wdumpStr("attachedTo", c.attachedTo);
        wdumpStr("mutuallyExclusiveWith", c.mutuallyExclusiveWith);

        // --- connection ---
        if (!c.connection.protocol.empty()) {
            cout << "  connection:\n";
            wdumpStr("protocol", c.connection.protocol, 4);
            wdumpInt("timeout", c.connection.timeout, 4);
            if (!c.connection.channels.empty()) {
                cout << "    channels (" << c.connection.channels.size() << ") :\n";
                for (const auto& chPair : c.connection.channels) {
                    cout << "      - " << chPair.first
                              << " address=" << chPair.second.address
                              << " port=" << chPair.second.port << "\n";
                }
            }
        }

        // --- axisMapping ---
        if (!c.axisMapping.empty()) {
            cout << "  axisMapping (" << c.axisMapping.size() << ") :\n";
            for (const auto& am : c.axisMapping) {
                cout << "    - controllerAxis=" << am.controllerAxis
                          << " component=" << am.component
                          << " axis=" << am.axis
                          << " role=" << am.role
                          << " countsPerUnit=" << am.countsPerUnit;
                if (!am.description.empty())
                    cout << " (" << am.description << ")";
                cout << "\n";
            }
        }

        // --- teachPoints ---
        if (!c.teachPoints.empty()) {
            cout << "  teachPoints (" << c.teachPoints.size() << ") :\n";
            for (const auto& tpPair : c.teachPoints)
                wdumpTeachPoint(tpPair.first, tpPair.second);
        }

        // --- overrides / params ---
        wdumpJson("overrides", c.overrides);
        wdumpJson("params", c.params);

        // --- ioPins ---
        if (!c.ioPins.empty()) {
            cout << "  ioPins (" << c.ioPins.size() << ") :\n";
            for (const auto& io : c.ioPins)
                wdumpIoPinEntry(io);
        }

        // --- position ---
        cout << "  position: x=" << c.position.x
                  << " y=" << c.position.y
                  << " z=" << c.position.z
                  << " theta=" << c.position.theta;
        if (!c.position.description.empty())
            cout << " (" << c.position.description << ")";
        cout << "\n";

        // --- approach ---
        cout << "  approach: offset=(" << c.approach.offset.x
                  << "," << c.approach.offset.y
                  << "," << c.approach.offset.z << ")"
                  << " speed=" << c.approach.speed;
        if (!c.approach.description.empty())
            cout << " (" << c.approach.description << ")";
        cout << "\n";

        // --- retract ---
        cout << "  retract: strategy=" << c.retract.strategy
                  << " clearance=" << c.retract.clearance;
        if (!c.retract.description.empty())
            cout << " (" << c.retract.description << ")";
        cout << "\n";

        // --- errorHandling ---
        cout << "  errorHandling: retryCount=" << c.errorHandling.retryCount
                  << " retryDelay=" << c.errorHandling.retryDelay
                  << " onFailure=" << c.errorHandling.onFailure
                  << " retractOnError=" << (c.errorHandling.retractOnError ? "true" : "false")
                  << " notifyTowerLight=" << (c.errorHandling.notifyTowerLight ? "true" : "false")
                  << " logLevel=" << c.errorHandling.logLevel;
        if (!c.errorHandling.description.empty())
            cout << " (" << c.errorHandling.description << ")";
        cout << "\n";

        // --- speeds ---
        cout << "  speeds: approach=" << c.speeds.approach
                  << " place=" << c.speeds.place
                  << " pick=" << c.speeds.pick;
        if (!c.speeds.description.empty())
            cout << " (" << c.speeds.description << ")";
        cout << "\n";

        // --- scanning ---
        cout << "  scanning: enabled=" << (c.scanning.enabled ? "true" : "false")
                  << " direction=" << c.scanning.direction
                  << " slots=[" << c.scanning.startSlot << ".." << c.scanning.endSlot << "]"
                  << " scanSpeed=" << c.scanning.scanSpeed
                  << " scanZ=[" << c.scanning.scanStartZ << ".." << c.scanning.scanEndZ << "]"
                  << " filterThreshold=" << c.scanning.filterThreshold;
        if (!c.scanning.description.empty())
            cout << " (" << c.scanning.description << ")";
        cout << "\n";

        // --- scanner ---
        cout << "  scanner: enabled=" << (c.scanner.enabled ? "true" : "false")
                  << " beamWidth=" << c.scanner.beamWidth
                  << " offsetFromTCP=(" << c.scanner.offsetFromTCP.x
                  << "," << c.scanner.offsetFromTCP.y
                  << "," << c.scanner.offsetFromTCP.z << ")";
        if (!c.scanner.description.empty())
            cout << " (" << c.scanner.description << ")";
        cout << "\n";
        if (!c.scanner.ioPins.empty()) {
            cout << "    scanner.ioPins (" << c.scanner.ioPins.size() << ") :\n";
            for (const auto& io : c.scanner.ioPins)
                wdumpIoPinEntry(io, 6);
        }

        // --- Component instance dump ---
        if (c.instance) {
            cout << "  --- Component instance dump ---\n";
            c.instance->dump();
        } else {
            cout << "  (no component instance loaded)\n";
        }
    }

    cout << "========================================\n";
    cout << "END WORKCELL DUMP\n";
    cout << "========================================\n";
}

// =============================================================================
// Assemblage et cycle de vie (V4 — nouveau)
// =============================================================================

bool Workcell::loadComponents(sol::state& lua)
{
    for (auto& entry : mComponents)
    {
        if (!entry.instance)
            continue;

        // 1. Enregistrer le type dans Lua (ex: Robot, Track...)
        entry.instance->registerInLua(lua);

        // 2. Exposer l'instance comme variable globale Lua
        //    Le nom de la variable = entry.id (ex: "robot", "track", "galil1")
        std::string varName = entry.id;

        // Associer l'instance à la variable globale selon son type
        if (entry.type == "controller")
            lua[varName] = dynamic_cast<Controller*>(entry.instance.get());
        else if (entry.type == "robot")
            lua[varName] = dynamic_cast<Robot*>(entry.instance.get());
        else if (entry.type == "linear_track")
            lua[varName] = dynamic_cast<Track*>(entry.instance.get());
        else if (entry.type == "pre_aligner")
            lua[varName] = dynamic_cast<Aligner*>(entry.instance.get());
        else if (entry.type == "flipper")
            lua[varName] = dynamic_cast<Flipper*>(entry.instance.get());
        else if (entry.type == "end_effector")
            lua[varName] = dynamic_cast<EndEffector*>(entry.instance.get());
        else if (entry.type == "signal_tower")
            lua[varName] = dynamic_cast<SignalTower*>(entry.instance.get());
        else if (entry.type == "cycle_counter")
            lua[varName] = dynamic_cast<CycleCounter*>(entry.instance.get());
        else if (entry.type == "cassette")
            lua[varName] = dynamic_cast<Cassette*>(entry.instance.get());
        else if (entry.type == "process_zone")
            lua[varName] = dynamic_cast<ProcessZone*>(entry.instance.get());
        else
            lua[varName] = entry.instance.get(); // fallback : Component*
    }

    mLastError.clear();
    return true;
}

bool Workcell::wireComponents(EventBus* bus)
{
    mEventBus = bus;

    // Passe 1 : instancier et connecter les contrôleurs
    for (auto& entry : mComponents)
    {
        if (entry.type != "controller")
            continue;

        Controller* controller = dynamic_cast<Controller*>(entry.instance.get());
        if (controller == nullptr)
        {
            mLastError = "Workcell::wireComponents: controller '" + entry.id
                        + "' is not a Controller instance";
            return false;
        }

        // Lire les canaux de connexion depuis la config WorkCell
        std::string address = "localhost";
        int commandPort = 2323;
        int messagePort = 2324;

        auto motionIt = entry.connection.channels.find("motion");
        if (motionIt != entry.connection.channels.end())
        {
            address = motionIt->second.address;
            commandPort = motionIt->second.port;
        }

        auto telemetryIt = entry.connection.channels.find("telemetry");
        if (telemetryIt != entry.connection.channels.end())
        {
            messagePort = telemetryIt->second.port;
        }

        // Ne pas continuer si aucun canal n'est configuré
        if (entry.connection.channels.empty())
        {
            mLastError = "Workcell::wireComponents: no connection channels for controller '"
                        + entry.id + "'";
            return false;
        }

        if (!controller->connect(address, commandPort, messagePort))
        {
            mLastError = "Workcell::wireComponents: failed to connect controller '"
                        + entry.id + "': " + controller->lastError();
            return false;
        }
    }

    // Passe 2 : injecter les Controller* dans les composants dépendants
    for (auto& entry : mComponents)
    {
        if (entry.type == "controller")
            continue;

        if (entry.controller.empty())
            continue;

        Component* ctrlComponent = findComponent(entry.controller);
        if (ctrlComponent == nullptr)
        {
            mLastError = "Workcell::wireComponents: controller '" + entry.controller
                        + "' not found for component '" + entry.id + "'";
            return false;
        }

        Controller* controller = dynamic_cast<Controller*>(ctrlComponent);
        if (controller == nullptr)
        {
            mLastError = "Workcell::wireComponents: '" + entry.controller
                        + "' is not a Controller";
            return false;
        }

        entry.instance->setController(controller);
    }

    mLastError.clear();
    return true;
}

void Workcell::startListeners()
{
    if (mEventBus == nullptr)
        return;

    for (auto& entry : mComponents)
    {
        if (entry.type != "controller")
            continue;

        Controller* controller = dynamic_cast<Controller*>(entry.instance.get());
        if (controller == nullptr)
            continue;

        GalilDriver* driver = controller->getDriver();
        if (driver == nullptr)
            continue;

        // Le listener tourne dans un thread dédié, lit le port 2324,
        // parse les messages asynchrones (MG, limites, erreurs) et
        // les publie dans l'EventBus.
        driver->startListener(mEventBus);
    }
}

void Workcell::shutdown()
{
    for (auto& entry : mComponents)
    {
        if (entry.type != "controller")
            continue;

        Controller* controller = dynamic_cast<Controller*>(entry.instance.get());
        if (controller == nullptr)
            continue;

        controller->disconnect();
    }

    mLastError.clear();
}
