#include <fstream>
#include <thread>
#include <chrono>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/EndEffector.h"
#include "Agent/Components/Controller.h"
#include "Agent/Drivers/GalilDriver.h"

using json = nlohmann::json;

EndEffector::EndEffector(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool EndEffector::loadFromFile(const std::string& filePath)
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
    type         = j.value("type", "");
    style        = j.value("style", "");
    description  = j.value("description", "");

    if (j.contains("units")) {
        auto& u = j["units"];
        units.length   = u.value("length", "");
        units.angle    = u.value("angle", "");
        units.pressure = u.value("pressure", "");
        units.force    = u.value("force", "");
        units.mass     = u.value("mass", "");
        units.time     = u.value("time", "");
    }

    if (j.contains("geometry")) {
        auto& g = j["geometry"];
        geometry.innerRadius = g.value("innerRadius", 0.0);
        geometry.outerRadius = g.value("outerRadius", 0.0);
        geometry.gapAngle    = g.value("gapAngle", 0.0);
        geometry.thickness   = g.value("thickness", 0.0);
        geometry.description = g.value("description", "");
    }

    if (j.contains("vacuum")) {
        auto& v = j["vacuum"];
        vacuum.channels     = v.value("channels", 0);
        vacuum.holdingForce = v.value("holdingForce", 0.0);
        vacuum.settleTime   = v.value("settleTime", 0);
        vacuum.threshold    = v.value("threshold", 0.0);
        vacuum.description  = v.value("description", "");
    }

    if (j.contains("waferCompatibility")) {
        auto& w = j["waferCompatibility"];
        waferCompatibility.maxWeight   = w.value("maxWeight", 0.0);
        waferCompatibility.description = w.value("description", "");
        if (w.contains("diameters")) {
            waferCompatibility.diameters.clear();
            for (const auto& d : w["diameters"])
                waferCompatibility.diameters.push_back(d.get<int>());
        }
    }

    if (j.contains("scanner")) {
        auto& s = j["scanner"];
        scanner.mountable     = s.value("mountable", false);
        scanner.mountPosition = s.value("mountPosition", "");
        scanner.scannerType   = s.value("scannerType", "");
        scanner.description   = s.value("description", "");
    }

    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.weight      = p.value("weight", 0.0);
        physical.material    = p.value("material", "");
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

    // --- ioPinsRequiredScanner ---
    if (j.contains("ioPinsRequiredScanner")) {
        ioPinsRequiredScanner.clear();
        for (const auto& jp : j["ioPinsRequiredScanner"]) {
            IoPinRequired ip;
            ip.id          = jp.value("id", "");
            ip.direction   = jp.value("direction", "");
            ip.signalType  = jp.value("signalType", "");
            ip.count       = jp.value("count", 1);
            ip.voltage     = jp.value("voltage", "");
            ip.debounceMs  = jp.value("debounceMs", 0);
            ip.description = jp.value("description", "");
            ioPinsRequiredScanner.push_back(ip);
        }
    }

    if (j.contains("metadata")) {
        auto& m = j["metadata"];
        if (m.contains("variants")) {
            metadata.variants.clear();
            for (const auto& v : m["variants"]) {
                EndEffectorVariant ev;
                ev.model       = v.value("model", "");
                ev.style       = v.value("style", "");
                ev.innerRadius = v.value("innerRadius", 0.0);
                ev.outerRadius = v.value("outerRadius", 0.0);
                ev.width       = v.value("width", 0.0);
                ev.length      = v.value("length", 0.0);
                ev.thickness   = v.value("thickness", 0.0);
                ev.description = v.value("description", "");
                if (v.contains("waferDiameters")) {
                    for (const auto& wd : v["waferDiameters"])
                        ev.waferDiameters.push_back(wd.get<int>());
                }
                metadata.variants.push_back(ev);
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
void EndEffector::dump() const
{
    Component::dump();

    std::cout << "  [EndEffector]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("type", type);
    dumpField("style", style);
    dumpField("description", description);

    std::cout << "    units:\n";
    dumpField("length", units.length, 6);
    dumpField("angle", units.angle, 6);
    dumpField("pressure", units.pressure, 6);
    dumpField("force", units.force, 6);
    dumpField("mass", units.mass, 6);
    dumpField("time", units.time, 6);

    std::cout << "    geometry:\n";
    dumpFieldDouble("innerRadius", geometry.innerRadius, 6);
    dumpFieldDouble("outerRadius", geometry.outerRadius, 6);
    dumpFieldDouble("gapAngle", geometry.gapAngle, 6);
    dumpFieldDouble("thickness", geometry.thickness, 6);
    dumpField("description", geometry.description, 6);

    std::cout << "    vacuum:\n";
    dumpFieldInt("channels", vacuum.channels, 6);
    dumpFieldDouble("holdingForce", vacuum.holdingForce, 6);
    dumpFieldInt("settleTime", vacuum.settleTime, 6);
    dumpFieldDouble("threshold", vacuum.threshold, 6);
    dumpField("description", vacuum.description, 6);

    std::cout << "    waferCompatibility:\n";
    dumpIntVector("diameters", waferCompatibility.diameters, 6);
    dumpFieldDouble("maxWeight", waferCompatibility.maxWeight, 6);
    dumpField("description", waferCompatibility.description, 6);

    std::cout << "    scanner:\n";
    dumpFieldBool("mountable", scanner.mountable, 6);
    dumpField("mountPosition", scanner.mountPosition, 6);
    dumpField("scannerType", scanner.scannerType, 6);
    dumpField("description", scanner.description, 6);

    std::cout << "    physical:\n";
    dumpFieldDouble("weight", physical.weight, 6);
    dumpField("material", physical.material, 6);
    dumpField("description", physical.description, 6);

    dumpIoPins(ioPinsRequired);
    dumpIoPins(ioPinsRequiredScanner, "ioPinsRequiredScanner");

    if (!metadata.variants.empty()) {
        std::cout << "    metadata.variants (" << metadata.variants.size() << ") :\n";
        for (const auto& v : metadata.variants) {
            std::cout << "      - model=" << v.model
                      << " style=" << v.style
                      << " innerRadius=" << v.innerRadius
                      << " outerRadius=" << v.outerRadius
                      << " width=" << v.width
                      << " length=" << v.length
                      << " thickness=" << v.thickness;
            if (!v.waferDiameters.empty()) {
                std::cout << " waferDiameters=[";
                for (size_t i = 0; i < v.waferDiameters.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << v.waferDiameters[i];
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

void EndEffector::setController(Controller* controller)
{
    mController = controller;
}

int EndEffector::getVacuumOutputPin() const
{
    // Chercher la pin vacuumOutput dans ioPinsRequired
    for (size_t i = 0; i < ioPinsRequired.size(); ++i)
    {
        const auto& io = ioPinsRequired[i];
        if (io.id.find("vacuumOutput") != std::string::npos
            || io.id.find("Vacuum") != std::string::npos)
        {
            return static_cast<int>(i); // l'index comme pin number
        }
    }
    return 0; // fallback
}

int EndEffector::getVacuumSensorPin() const
{
    for (size_t i = 0; i < ioPinsRequired.size(); ++i)
    {
        const auto& io = ioPinsRequired[i];
        if (io.id.find("vacuumSensor") != std::string::npos
            || io.id.find("Sensor") != std::string::npos)
        {
            return static_cast<int>(i);
        }
    }
    return 1; // fallback
}

bool EndEffector::pick(int settleMs)
{
    if (mController == nullptr) {
        mLastError = "EndEffector::pick: no controller";
        return false;
    }

    int pin = getVacuumOutputPin();

    if (!mController->setDigitalOutput(pin, true)) {
        mLastError = "EndEffector::pick: failed to activate vacuum pin "
                     + std::to_string(pin);
        return false;
    }

    // Temps de stabilisation du vide
    std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));

    return true;
}

bool EndEffector::place(int settleMs)
{
    if (mController == nullptr) {
        mLastError = "EndEffector::place: no controller";
        return false;
    }

    int pin = getVacuumOutputPin();

    if (!mController->setDigitalOutput(pin, false)) {
        mLastError = "EndEffector::place: failed to deactivate vacuum pin "
                     + std::to_string(pin);
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));

    return true;
}

bool EndEffector::isWaferPresent()
{
    if (mController == nullptr) {
        return false;
    }

    int pin = getVacuumSensorPin();
    bool value = false;

    // Le capteur de vide est une entrée digitale
    // On utilise waitDigitalInput avec timeout 0 = lecture immédiate
    if (!mController->waitDigitalInput(pin, true, 0)) {
        // Si le wait échoue (timeout immédiat), le wafer n'est pas présent
        return false;
    }

    return value;
}

void EndEffector::registerInLua(sol::state& lua)
{
    lua.new_usertype<EndEffector>("EndEffector",
        sol::constructors<EndEffector()>(),
        "model",         sol::readonly(&EndEffector::model),
        "manufacturer",  sol::readonly(&EndEffector::manufacturer),
        "loadFromFile",  &EndEffector::loadFromFile,
        "pick",          &EndEffector::pick,
        "place",         &EndEffector::place,
        "isWaferPresent",&EndEffector::isWaferPresent,
        "lastError",     &EndEffector::lastError,
        "dump",          &EndEffector::dump
    );
}
