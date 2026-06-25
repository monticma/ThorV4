#include <fstream>
#include <thread>
#include <chrono>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/Aligner.h"
#include "Agent/Components/Controller.h"
#include "Agent/Drivers/GalilDriver.h"

using json = nlohmann::json;

Aligner::Aligner(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool Aligner::loadFromFile(const std::string& filePath)
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

    // --- Champs communs (Component) ---
    version  = j.value("version", "");
    fileType = j.value("fileType", "");

    // --- Identification ---
    model        = j.value("model", "");
    manufacturer = j.value("manufacturer", "");
    description  = j.value("description", "");

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
    }

    // --- Racine ---
    axisCount = j.value("axisCount", 0);
    loadType  = j.value("loadType", "");

    // --- axes (format standardisé) ---
    if (j.contains("axes")) {
        axes.clear();
        for (const auto& ja : j["axes"]) {
            ComponentAxis ax;
            ax.name           = ja.value("name", "");
            ax.type           = ja.value("type", "");
            ax.role           = ja.value("role", "");
            ax.description    = ja.value("description", "");
            ax.travel.min        = ja["travel"].value("min", 0.0);
            ax.travel.max        = ja["travel"].value("max", 0.0);
            ax.travel.continuous = ja["travel"].value("continuous", false);
            ax.maxVelocity     = ja.value("maxVelocity", 0.0);
            ax.maxAcceleration = ja.value("maxAcceleration", 0.0);
            ax.homePosition    = ja.value("homePosition", 0.0);
            ax.upPosition      = ja.value("upPosition", 0.0);
            ax.downPosition    = ja.value("downPosition", 0.0);
            axes.push_back(ax);
        }
    }

    // --- homing ---
    if (j.contains("homing")) {
        auto& h = j["homing"];
        homing.method      = h.value("method", "");
        homing.direction   = h.value("direction", "");
        homing.velocity    = h.value("velocity", 0.0);
        homing.description = h.value("description", "");
        if (h.contains("sequence")) {
            homing.sequence.clear();
            for (const auto& s : h["sequence"])
                homing.sequence.push_back(s.get<std::string>());
        }
    }

    // --- alignment ---
    if (j.contains("alignment")) {
        auto& al = j["alignment"];
        alignment.autoDetect       = al.value("autoDetect", true);
        alignment.defaultWaferSize = al.value("defaultWaferSize", 200);
        alignment.sensorType       = al.value("sensorType", "");
        alignment.lightSource      = al.value("lightSource", "");
        alignment.settleTime       = al.value("settleTime", 300);
        alignment.maxAlignmentTime = al.value("maxAlignmentTime", 15000);
        alignment.repeatability    = al.value("repeatability", 0.0);
        alignment.repeatabilityUnit = al.value("repeatabilityUnit", "");
        alignment.description      = al.value("description", "");

        if (al.contains("modes")) {
            alignment.modes.clear();
            for (const auto& v : al["modes"])
                alignment.modes.push_back(v.get<std::string>());
        }
        if (al.contains("waferSizes")) {
            alignment.waferSizes.clear();
            for (const auto& v : al["waferSizes"])
                alignment.waferSizes.push_back(v.get<int>());
        }
    }

    // --- visual (chemin vers un .visual.json) ---
    visual = j.value("visual", "");

    // --- pinAssembly ---
    if (j.contains("pinAssembly")) {
        auto& pa = j["pinAssembly"];
        pinAssembly.pinCount       = pa.value("pinCount", 3);
        pinAssembly.pinType        = pa.value("pinType", "");
        pinAssembly.vacuumAssisted = pa.value("vacuumAssisted", true);
        pinAssembly.description    = pa.value("description", "");
    }

    // --- compatibleEndEffectors ---
    if (j.contains("compatibleEndEffectors")) {
        compatibleEndEffectors.clear();
        for (const auto& v : j["compatibleEndEffectors"])
            compatibleEndEffectors.push_back(v.get<std::string>());
    }

    // --- physical ---
    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.chuckDiameter   = p.value("chuckDiameter", 0.0);
        physical.bodyDiameter    = p.value("bodyDiameter", 0.0);
        physical.height          = p.value("height", 0.0);
        physical.weight          = p.value("weight", 0.0);
        physical.weightUnit      = p.value("weightUnit", "");
        physical.mountingPattern = p.value("mountingPattern", "");
        physical.description     = p.value("description", "");
    }

    // --- ioPinsRequired ---
    if (j.contains("ioPinsRequired")) {
        ioPinsRequired.clear();
        for (const auto& jp : j["ioPinsRequired"]) {
            IoPinRequired ip;
            ip.id          = jp.value("id", "");
            ip.direction   = jp.value("direction", "");
            ip.signalType  = jp.value("signalType", "");
            ip.count       = jp.value("count", 1);
            ip.voltage     = jp.value("voltage", "");
            ip.debounceMs  = jp.value("debounceMs", 0);
            ip.description = jp.value("description", "");
            ioPinsRequired.push_back(ip);
        }
    }

    // --- metadata ---
    if (j.contains("metadata")) {
        auto& m = j["metadata"];
        if (m.contains("variants")) {
            metadata.variants.clear();
            for (const auto& v : m["variants"]) {
                AlignerVariant av;
                av.model       = v.value("model", "");
                av.axisCount   = v.value("axisCount", 0);
                av.loadType    = v.value("loadType", "");
                av.description = v.value("description", "");
                if (v.contains("waferSizes")) {
                    for (const auto& ws : v["waferSizes"])
                        av.waferSizes.push_back(ws.get<int>());
                }
                metadata.variants.push_back(av);
            }
        }
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
void Aligner::dump() const
{
    Component::dump();

    std::cout << "  [Aligner]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("description", description);

    dumpFieldInt("axisCount", axisCount);
    dumpField("loadType", loadType);

    std::cout << "    units:\n";
    dumpField("length", units.length, 6);
    dumpField("angle", units.angle, 6);
    dumpField("linearVelocity", units.linearVelocity, 6);
    dumpField("angularVelocity", units.angularVelocity, 6);
    dumpField("linearAcceleration", units.linearAcceleration, 6);
    dumpField("angularAcceleration", units.angularAcceleration, 6);
    dumpField("time", units.time, 6);

    dumpAxes(axes);
    dumpHoming(homing);

    std::cout << "    alignment:\n";
    dumpStringVector("modes", alignment.modes, 6);
    dumpFieldBool("autoDetect", alignment.autoDetect, 6);
    dumpIntVector("waferSizes", alignment.waferSizes, 6);
    dumpFieldInt("defaultWaferSize", alignment.defaultWaferSize, 6);
    dumpField("sensorType", alignment.sensorType, 6);
    dumpField("lightSource", alignment.lightSource, 6);
    dumpFieldInt("settleTime", alignment.settleTime, 6);
    dumpFieldInt("maxAlignmentTime", alignment.maxAlignmentTime, 6);
    dumpFieldDouble("repeatability", alignment.repeatability, 6);
    dumpField("repeatabilityUnit", alignment.repeatabilityUnit, 6);
    dumpField("description", alignment.description, 6);

    dumpField("visual", visual);

    std::cout << "    pinAssembly:\n";
    dumpFieldInt("pinCount", pinAssembly.pinCount, 6);
    dumpField("pinType", pinAssembly.pinType, 6);
    dumpFieldBool("vacuumAssisted", pinAssembly.vacuumAssisted, 6);
    dumpField("description", pinAssembly.description, 6);

    dumpStringVector("compatibleEndEffectors", compatibleEndEffectors);

    std::cout << "    physical:\n";
    dumpFieldDouble("chuckDiameter", physical.chuckDiameter, 6);
    dumpFieldDouble("bodyDiameter", physical.bodyDiameter, 6);
    dumpFieldDouble("height", physical.height, 6);
    dumpFieldDouble("weight", physical.weight, 6);
    dumpField("weightUnit", physical.weightUnit, 6);
    dumpField("mountingPattern", physical.mountingPattern, 6);
    dumpField("description", physical.description, 6);

    dumpIoPins(ioPinsRequired);

    if (!metadata.variants.empty()) {
        std::cout << "    metadata.variants (" << metadata.variants.size() << ") :\n";
        for (const auto& v : metadata.variants) {
            std::cout << "      - model=" << v.model
                      << " axisCount=" << v.axisCount
                      << " loadType=" << v.loadType;
            if (!v.waferSizes.empty()) {
                std::cout << " waferSizes=[";
                for (size_t i = 0; i < v.waferSizes.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << v.waferSizes[i];
                }
                std::cout << "]";
            }
            if (!v.description.empty())
                std::cout << " (" << v.description << ")";
            std::cout << "\n";
        }
    }
    dumpStringVector("metadata.notes", metadata.notes, 4);
}

// =============================================================================
// Primitives
// =============================================================================

void Aligner::setController(Controller* controller)
{
    mController = controller;
}

bool Aligner::moveTo(double angle, double speed)
{
    if (mController == nullptr) {
        mLastError = "Aligner::moveTo: no controller";
        return false;
    }

    // L'aligneur a typiquement 1 axe de rotation (chuck)
    // angle en degrés, convertir en counts via le driver
    std::vector<double> counts(8, 0.0);
    counts[0] = angle; // axe primaire, countsPerUnit appliqué par le driver

    double speedFactor = speed / 100.0;
    if (speedFactor < 0.01) speedFactor = 0.01;
    if (speedFactor > 1.0)  speedFactor = 1.0;

    std::vector<double> speedCounts(8, 0.0);
    if (!axes.empty()) {
        speedCounts[0] = axes[0].maxVelocity * speedFactor;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Aligner::moveTo: driver not available";
        return false;
    }

    driver->setSpeeds(speedCounts);
    return driver->moveAbsolute(counts);
}

bool Aligner::home()
{
    if (mController == nullptr) {
        mLastError = "Aligner::home: no controller";
        return false;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Aligner::home: driver not available";
        return false;
    }

    // Homing de tous les axes dans l'ordre défini
    for (size_t i = 0; i < homing.sequence.size(); ++i) {
        const std::string& axisName = homing.sequence[i];
        int axisIndex = -1;
        for (size_t j = 0; j < axes.size(); ++j) {
            if (axes[j].name == axisName) {
                axisIndex = static_cast<int>(j);
                break;
            }
        }
        if (axisIndex < 0) {
            mLastError = "Aligner::home: unknown axis '" + axisName + "'";
            return false;
        }
        if (!driver->home(axisIndex)) {
            mLastError = "Aligner::home: failed for axis " + axisName
                         + ": " + driver->lastError();
            return false;
        }
    }
    return true;
}

bool Aligner::stopMotion()
{
    if (mController == nullptr) return false;
    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) return false;
    return driver->stopAll();
}

bool Aligner::emergencyStop()
{
    if (mController == nullptr) return false;
    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) return false;
    driver->stopAll();
    driver->motorOff();
    return true;
}

bool Aligner::alignWafer(double angle, int settleMs)
{
    // 1. Tourner le chuck à l'angle cible
    if (!moveTo(angle, 30.0)) {
        return false;
    }

    // 2. Attendre la fin du mouvement
    GalilDriver* driver = mController->getDriver();
    if (driver != nullptr) {
        // Attendre que le mouvement soit terminé (polling sur le port 2323)
        // Pour l'instant, on attend juste le settle time
    }

    // 3. Temps de stabilisation (settle)
    std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));

    // 4. L'alignement optique est géré par le hardware de l'aligneur
    //    (le capteur through-beam détecte le flat/notch)
    return true;
}

void Aligner::registerInLua(sol::state& lua)
{
    lua.new_usertype<Aligner>("Aligner",
        sol::constructors<Aligner()>(),
        "model",         sol::readonly(&Aligner::model),
        "manufacturer",  sol::readonly(&Aligner::manufacturer),
        "loadFromFile",  &Aligner::loadFromFile,
        "moveTo",        &Aligner::moveTo,
        "home",          &Aligner::home,
        "stopMotion",    &Aligner::stopMotion,
        "emergencyStop", &Aligner::emergencyStop,
        "alignWafer",    &Aligner::alignWafer,
        "lastError",     &Aligner::lastError,
        "dump",          &Aligner::dump
    );
}
