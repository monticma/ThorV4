#include <fstream>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/Flipper.h"
#include "Agent/Components/Controller.h"
#include "Agent/Drivers/GalilDriver.h"

using json = nlohmann::json;

Flipper::Flipper(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool Flipper::loadFromFile(const std::string& filePath)
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

    version  = j.value("version", "");
    fileType = j.value("fileType", "");

    model        = j.value("model", "");
    manufacturer = j.value("manufacturer", "");
    description  = j.value("description", "");

    if (j.contains("units")) {
        auto& u = j["units"];
        units.angle              = u.value("angle", "");
        units.angularVelocity    = u.value("angularVelocity", "");
        units.angularAcceleration = u.value("angularAcceleration", "");
    }

    axisCount = j.value("axisCount", 0);

    if (j.contains("axes")) {
        axes.clear();
        for (const auto& ja : j["axes"]) {
            ComponentAxis ax;
            ax.name           = ja.value("name", "");
            ax.type           = ja.value("type", "");
            ax.role           = ja.value("role", "");
            ax.description    = ja.value("description", "");
            ax.travel.min     = ja["travel"].value("min", 0.0);
            ax.travel.max     = ja["travel"].value("max", 0.0);
            ax.maxVelocity     = ja.value("maxVelocity", 0.0);
            ax.maxAcceleration = ja.value("maxAcceleration", 0.0);
            axes.push_back(ax);
        }
    }

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

    visual = j.value("visual", "");

    if (j.contains("vacuum")) {
        auto& v = j["vacuum"];
        vacuum.topChuck    = v.value("topChuck", true);
        vacuum.bottomChuck = v.value("bottomChuck", true);
        vacuum.description = v.value("description", "");
    }

    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.width       = p.value("width", 0.0);
        physical.depth       = p.value("depth", 0.0);
        physical.height      = p.value("height", 0.0);
        physical.weight      = p.value("weight", 0.0);
        physical.weightUnit  = p.value("weightUnit", "");
        physical.description = p.value("description", "");
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

    if (j.contains("metadata") && j["metadata"].contains("notes")) {
        metadata.notes.clear();
        for (const auto& n : j["metadata"]["notes"])
            metadata.notes.push_back(n.get<std::string>());
    }

    mLastError.clear();
    return true;
}

// -----------------------------------------------------------------------------
void Flipper::dump() const
{
    Component::dump();

    std::cout << "  [Flipper]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("description", description);

    std::cout << "    units:\n";
    dumpField("angle", units.angle, 6);
    dumpField("angularVelocity", units.angularVelocity, 6);
    dumpField("angularAcceleration", units.angularAcceleration, 6);

    dumpFieldInt("axisCount", axisCount);
    dumpAxes(axes);
    dumpHoming(homing);
    dumpField("visual", visual);

    std::cout << "    vacuum:\n";
    dumpFieldBool("topChuck", vacuum.topChuck, 6);
    dumpFieldBool("bottomChuck", vacuum.bottomChuck, 6);
    dumpField("description", vacuum.description, 6);

    std::cout << "    physical:\n";
    dumpFieldDouble("width", physical.width, 6);
    dumpFieldDouble("depth", physical.depth, 6);
    dumpFieldDouble("height", physical.height, 6);
    dumpFieldDouble("weight", physical.weight, 6);
    dumpField("weightUnit", physical.weightUnit, 6);
    dumpField("description", physical.description, 6);

    dumpIoPins(ioPinsRequired);

    dumpStringVector("metadata.notes", metadata.notes, 4);
}

// =============================================================================
// Primitives
// =============================================================================

void Flipper::setController(Controller* controller)
{
    mController = controller;
}

bool Flipper::flip(double speed)
{
    if (mController == nullptr) {
        mLastError = "Flipper::flip: no controller";
        return false;
    }

    // Le flipper a 1 axe rotatif — rotation de 180° pour inverser le wafer
    std::vector<double> counts(8, 0.0);
    counts[0] = 180.0; // 180 degrés

    double speedFactor = speed / 100.0;
    if (speedFactor < 0.01) speedFactor = 0.01;
    if (speedFactor > 1.0)  speedFactor = 1.0;

    std::vector<double> speedCounts(8, 0.0);
    if (!axes.empty()) {
        speedCounts[0] = axes[0].maxVelocity * speedFactor;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Flipper::flip: driver not available";
        return false;
    }

    driver->setSpeeds(speedCounts);
    return driver->moveAbsolute(counts);
}

bool Flipper::home()
{
    if (mController == nullptr) {
        mLastError = "Flipper::home: no controller";
        return false;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Flipper::home: driver not available";
        return false;
    }

    // Homing sur l'axe unique du flipper
    return driver->home(0);
}

bool Flipper::stopMotion()
{
    if (mController == nullptr) return false;
    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) return false;
    return driver->stopAll();
}

bool Flipper::emergencyStop()
{
    if (mController == nullptr) return false;
    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) return false;
    driver->stopAll();
    driver->motorOff();
    return true;
}

void Flipper::registerInLua(sol::state& lua)
{
    lua.new_usertype<Flipper>("Flipper",
        sol::constructors<Flipper()>(),
        "model",         sol::readonly(&Flipper::model),
        "manufacturer",  sol::readonly(&Flipper::manufacturer),
        "loadFromFile",  &Flipper::loadFromFile,
        "flip",          &Flipper::flip,
        "home",          &Flipper::home,
        "stopMotion",    &Flipper::stopMotion,
        "emergencyStop", &Flipper::emergencyStop,
        "lastError",     &Flipper::lastError,
        "dump",          &Flipper::dump
    );
}
