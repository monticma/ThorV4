#include <fstream>
#include <thread>
#include <chrono>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/Controller.h"
#include "Agent/Drivers/GalilDriver.h"

using json = nlohmann::json;

// -----------------------------------------------------------------------------
Controller::Controller(const std::string& filePath)
{
    loadFromFile(filePath);
}

// -----------------------------------------------------------------------------
bool Controller::loadFromFile(const std::string& filePath)
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
    family       = j.value("family", "");
    description  = j.value("description", "");

    // --- units ---
    if (j.contains("units")) {
        auto& u = j["units"];
        units.length             = u.value("length", "");
        units.angle              = u.value("angle", "");
        units.linearVelocity     = u.value("linearVelocity", "");
        units.angularVelocity    = u.value("angularVelocity", "");
        units.linearAcceleration = u.value("linearAcceleration", "");
        units.angularAcceleration = u.value("angularAcceleration", "");
        units.time               = u.value("time", "");
    }

    // --- capabilities ---
    if (j.contains("capabilities")) {
        auto& c = j["capabilities"];
        capabilities.maxAxes             = c.value("maxAxes", 0);
        capabilities.coordinatedMotion    = c.value("coordinatedMotion", false);
        capabilities.linearInterpolation  = c.value("linearInterpolation", false);
        capabilities.circularInterpolation = c.value("circularInterpolation", false);
        capabilities.electronicGearing    = c.value("electronicGearing", false);
        capabilities.encoderFeedback      = c.value("encoderFeedback", false);
        capabilities.pidControl           = c.value("pidControl", false);
        capabilities.digitalInputs        = c.value("digitalInputs", 0);
        capabilities.digitalOutputs       = c.value("digitalOutputs", 0);
        capabilities.analogInputs         = c.value("analogInputs", 0);
        capabilities.analogOutputs        = c.value("analogOutputs", 0);
        capabilities.programMemory        = c.value("programMemory", 0);
        capabilities.firmwareLanguage     = c.value("firmwareLanguage", "");

        if (c.contains("axisLabels")) {
            capabilities.axisLabels.clear();
            for (const auto& v : c["axisLabels"])
                capabilities.axisLabels.push_back(v.get<std::string>());
        }
        if (c.contains("motionProfiles")) {
            capabilities.motionProfiles.clear();
            for (const auto& v : c["motionProfiles"])
                capabilities.motionProfiles.push_back(v.get<std::string>());
        }

        // encoderResolution (imbriqué)
        if (c.contains("encoderResolution")) {
            auto& er = c["encoderResolution"];
            capabilities.encoderResolution.min  = er.value("min", 0);
            capabilities.encoderResolution.max  = er.value("max", 0);
            capabilities.encoderResolution.unit = er.value("unit", "");
        }
    }

    // --- connection ---
    if (j.contains("connection")) {
        auto& co = j["connection"];
        if (co.contains("supportedProtocols")) {
            connection.supportedProtocols.clear();
            for (const auto& v : co["supportedProtocols"])
                connection.supportedProtocols.push_back(v.get<std::string>());
        }

        // tcp
        if (co.contains("tcp")) {
            auto& t = co["tcp"];
            connection.tcp.defaultPort = t.value("defaultPort", 0);
            connection.tcp.commandPort = t.value("commandPort", 0);
            connection.tcp.messagePort = t.value("messagePort", 0);
            connection.tcp.description = t.value("description", "");
        }

        // serial
        if (co.contains("serial")) {
            auto& s = co["serial"];
            connection.serial.defaultBaud = s.value("defaultBaud", 0);
            connection.serial.dataBits    = s.value("dataBits", 0);
            connection.serial.parity      = s.value("parity", "");
            connection.serial.stopBits    = s.value("stopBits", 0);
        }
    }

    // --- driver ---
    if (j.contains("driver")) {
        auto& d = j["driver"];
        driver.identifier  = d.value("identifier", "");
        driver.library     = d.value("library", "");
        driver.driverConfig = d.value("driverConfig", "");
        driver.description = d.value("description", "");
    }

    // --- limits ---
    if (j.contains("limits")) {
        auto& l = j["limits"];
        limits.maxVelocity     = l.value("maxVelocity", 0.0);
        limits.maxAcceleration = l.value("maxAcceleration", 0.0);
        limits.velocityUnit    = l.value("velocityUnit", "");
        limits.accelerationUnit = l.value("accelerationUnit", "");
        limits.description     = l.value("description", "");
    }

    // --- ioCapabilities ---
    if (j.contains("ioCapabilities")) {
        auto& ic = j["ioCapabilities"];
        ioCapabilities.digitalInputs      = ic.value("digitalInputs", 0);
        ioCapabilities.digitalOutputs     = ic.value("digitalOutputs", 0);
        ioCapabilities.analogInputs       = ic.value("analogInputs", 0);
        ioCapabilities.analogOutputs      = ic.value("analogOutputs", 0);
        ioCapabilities.digitalVoltage     = ic.value("digitalVoltage", "");
        ioCapabilities.analogVoltage      = ic.value("analogVoltage", "");
        ioCapabilities.maxCurrentPerOutput = ic.value("maxCurrentPerOutput", 0);
        ioCapabilities.currentUnit        = ic.value("currentUnit", "");
        ioCapabilities.description        = ic.value("description", "");
    }

    // --- metadata ---
    if (j.contains("metadata")) {
        auto& m = j["metadata"];
        metadata.datasheet = m.value("datasheet", "");
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
void Controller::dump() const
{
    Component::dump();

    std::cout << "  [Controller]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("family", family);
    dumpField("description", description);

    std::cout << "    units:\n";
    dumpField("length", units.length, 6);
    dumpField("angle", units.angle, 6);
    dumpField("linearVelocity", units.linearVelocity, 6);
    dumpField("angularVelocity", units.angularVelocity, 6);
    dumpField("linearAcceleration", units.linearAcceleration, 6);
    dumpField("angularAcceleration", units.angularAcceleration, 6);
    dumpField("time", units.time, 6);

    std::cout << "    capabilities:\n";
    dumpFieldInt("maxAxes", capabilities.maxAxes, 6);
    dumpStringVector("axisLabels", capabilities.axisLabels, 6);
    dumpFieldBool("coordinatedMotion", capabilities.coordinatedMotion, 6);
    dumpFieldBool("linearInterpolation", capabilities.linearInterpolation, 6);
    dumpFieldBool("circularInterpolation", capabilities.circularInterpolation, 6);
    dumpFieldBool("electronicGearing", capabilities.electronicGearing, 6);
    dumpFieldBool("encoderFeedback", capabilities.encoderFeedback, 6);
    std::cout << "      encoderResolution: min=" << capabilities.encoderResolution.min
              << " max=" << capabilities.encoderResolution.max
              << " unit=" << capabilities.encoderResolution.unit << "\n";
    dumpFieldBool("pidControl", capabilities.pidControl, 6);
    dumpStringVector("motionProfiles", capabilities.motionProfiles, 6);
    dumpFieldInt("digitalInputs", capabilities.digitalInputs, 6);
    dumpFieldInt("digitalOutputs", capabilities.digitalOutputs, 6);
    dumpFieldInt("analogInputs", capabilities.analogInputs, 6);
    dumpFieldInt("analogOutputs", capabilities.analogOutputs, 6);
    dumpFieldInt("programMemory", capabilities.programMemory, 6);
    dumpField("firmwareLanguage", capabilities.firmwareLanguage, 6);

    std::cout << "    connection:\n";
    dumpStringVector("supportedProtocols", connection.supportedProtocols, 6);
    std::cout << "      tcp: defaultPort=" << connection.tcp.defaultPort
              << " commandPort=" << connection.tcp.commandPort
              << " messagePort=" << connection.tcp.messagePort;
    if (!connection.tcp.description.empty())
        std::cout << " (" << connection.tcp.description << ")";
    std::cout << "\n";
    std::cout << "      serial: defaultBaud=" << connection.serial.defaultBaud
              << " dataBits=" << connection.serial.dataBits
              << " parity=" << connection.serial.parity
              << " stopBits=" << connection.serial.stopBits << "\n";

    std::cout << "    driver:\n";
    dumpField("identifier", driver.identifier, 6);
    dumpField("library", driver.library, 6);
    dumpField("driverConfig", driver.driverConfig, 6);
    dumpField("description", driver.description, 6);

    std::cout << "    limits:\n";
    dumpFieldDouble("maxVelocity", limits.maxVelocity, 6);
    dumpFieldDouble("maxAcceleration", limits.maxAcceleration, 6);
    dumpField("velocityUnit", limits.velocityUnit, 6);
    dumpField("accelerationUnit", limits.accelerationUnit, 6);
    dumpField("description", limits.description, 6);

    std::cout << "    ioCapabilities:\n";
    dumpFieldInt("digitalInputs", ioCapabilities.digitalInputs, 6);
    dumpFieldInt("digitalOutputs", ioCapabilities.digitalOutputs, 6);
    dumpFieldInt("analogInputs", ioCapabilities.analogInputs, 6);
    dumpFieldInt("analogOutputs", ioCapabilities.analogOutputs, 6);
    dumpField("digitalVoltage", ioCapabilities.digitalVoltage, 6);
    dumpField("analogVoltage", ioCapabilities.analogVoltage, 6);
    dumpFieldInt("maxCurrentPerOutput", ioCapabilities.maxCurrentPerOutput, 6);
    dumpField("currentUnit", ioCapabilities.currentUnit, 6);
    dumpField("description", ioCapabilities.description, 6);

    std::cout << "    metadata:\n";
    dumpField("datasheet", metadata.datasheet, 6);
    dumpStringVector("notes", metadata.notes, 6);
}

// =============================================================================
// Opérations (nouveau)
// =============================================================================

bool Controller::connect(const std::string& address, int commandPort, int messagePort)
{
    if (!mDriver.connect(address, commandPort, messagePort))
    {
        mLastError = mDriver.lastError();
        return false;
    }
    return true;
}

void Controller::disconnect()
{
    mDriver.disconnect();
}

bool Controller::isConnected() const
{
    return mDriver.isConnected();
}

GalilDriver* Controller::getDriver()
{
    return &mDriver;
}

void Controller::registerInLua(sol::state& lua)
{
    lua.new_usertype<Controller>("Controller",
        sol::constructors<Controller()>(),
        "model",        sol::readonly(&Controller::model),
        "manufacturer", sol::readonly(&Controller::manufacturer),
        "connect",      &Controller::connect,
        "disconnect",   &Controller::disconnect,
        "isConnected",  &Controller::isConnected,
        "loadFromFile", &Controller::loadFromFile,
        "lastError",    &Controller::lastError,
        "dump",         &Controller::dump
    );
}

// =============================================================================
// IO Primitives
// =============================================================================

bool Controller::setDigitalOutput(int pin, bool value)
{
    return mDriver.setDigitalOutput(pin, value);
}

bool Controller::waitDigitalInput(int pin, bool expected, int timeoutMs)
{
    // Polling loop — le listener Galil (port 2324) peut interrompre
    // en publiant un événement, mais pour l'instant on poll.
    int elapsed = 0;
    const int pollIntervalMs = 10;

    while (elapsed < timeoutMs)
    {
        bool current = false;
        if (!mDriver.getDigitalInput(pin, current))
        {
            mLastError = "Controller::waitDigitalInput: " + mDriver.lastError();
            return false;
        }
        if (current == expected)
        {
            return true;
        }
        // Pause minimale entre deux polls (10 ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }

    mLastError = "Controller::waitDigitalInput: timeout waiting for pin "
               + std::to_string(pin) + " to become "
               + (expected ? "HIGH" : "LOW");
    return false;
}

bool Controller::setAnalogOutput(int channel, double value)
{
    (void)channel;
    (void)value;
    mLastError = "Controller::setAnalogOutput: not supported by Galil driver";
    return false;
}

bool Controller::readAnalogInput(int channel, double& valueOut)
{
    (void)channel;
    valueOut = 0.0;
    mLastError = "Controller::readAnalogInput: not supported by Galil driver";
    return false;
}
