#include <fstream>
#include <thread>
#include <chrono>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/ProcessZone.h"
#include "Agent/Components/Controller.h"

using json = nlohmann::json;

ProcessZone::ProcessZone(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool ProcessZone::loadFromFile(const std::string& filePath)
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

    if (j.contains("protocol")) {
        auto& pr = j["protocol"];
        protocol.type        = pr.value("type", "");
        protocol.description = pr.value("description", "");
        if (pr.contains("signals")) {
            for (auto& [key, val] : pr["signals"].items()) {
                ProcessZoneSignal sig;
                sig.direction   = val.value("direction", "");
                sig.description = val.value("description", "");
                protocol.signals[key] = sig;
            }
        }
    }

    if (j.contains("defaults")) {
        auto& d = j["defaults"];
        defaults.timeout     = d.value("timeout", 30000);
        defaults.description = d.value("description", "");
    }

    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.chamberType = p.value("chamberType", "");
        physical.description = p.value("description", "");
        if (p.contains("waferSizes")) {
            physical.waferSizes.clear();
            for (const auto& ws : p["waferSizes"])
                physical.waferSizes.push_back(ws.get<int>());
        }
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

    if (j.contains("metadata")) {
        auto& m = j["metadata"];
        if (m.contains("variants")) {
            metadata.variants.clear();
            for (const auto& v : m["variants"]) {
                ProcessZoneVariant pv;
                pv.model       = v.value("model", "");
                pv.chamberType = v.value("chamberType", "");
                pv.description = v.value("description", "");
                metadata.variants.push_back(pv);
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
void ProcessZone::dump() const
{
    Component::dump();

    std::cout << "  [ProcessZone]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("description", description);

    std::cout << "    protocol:\n";
    dumpField("type", protocol.type, 6);
    dumpField("description", protocol.description, 6);
    if (!protocol.signals.empty()) {
        std::cout << "      signals (" << protocol.signals.size() << ") :\n";
        for (const auto& [key, sig] : protocol.signals) {
            std::cout << "        - " << key
                      << " dir=" << sig.direction;
            if (!sig.description.empty())
                std::cout << " (" << sig.description << ")";
            std::cout << "\n";
        }
    }

    std::cout << "    defaults:\n";
    dumpFieldInt("timeout", defaults.timeout, 6);
    dumpField("description", defaults.description, 6);

    std::cout << "    physical:\n";
    dumpField("chamberType", physical.chamberType, 6);
    dumpIntVector("waferSizes", physical.waferSizes, 6);
    dumpField("description", physical.description, 6);

    dumpIoPins(ioPinsRequired);

    if (!metadata.variants.empty()) {
        std::cout << "    metadata.variants (" << metadata.variants.size() << ") :\n";
        for (const auto& v : metadata.variants) {
            std::cout << "      - model=" << v.model
                      << " chamberType=" << v.chamberType;
            if (!v.description.empty())
                std::cout << " (" << v.description << ")";
            std::cout << "\n";
        }
    }
    dumpStringVector("metadata.notes", metadata.notes, 4);
}

// =============================================================================
// Primitives (handshake I/O 5-wire)
// =============================================================================

void ProcessZone::setController(Controller* controller)
{
    mController = controller;
}

int ProcessZone::getIoPin(const std::string& idPattern) const
{
    for (size_t i = 0; i < ioPinsRequired.size(); ++i)
    {
        if (ioPinsRequired[i].id.find(idPattern) != std::string::npos)
            return static_cast<int>(i);
    }
    return -1;
}

bool ProcessZone::isReady()
{
    if (mController == nullptr) {
        mLastError = "ProcessZone::isReady: no controller";
        return false;
    }

    int pin = getIoPin("ready");
    if (pin < 0) {
        mLastError = "ProcessZone::isReady: no 'ready' pin configured";
        return false;
    }

    return mController->waitDigitalInput(pin, true, 0);
}

bool ProcessZone::startProcess()
{
    if (mController == nullptr) {
        mLastError = "ProcessZone::startProcess: no controller";
        return false;
    }

    int pin = getIoPin("startProcess");
    if (pin < 0) {
        mLastError = "ProcessZone::startProcess: no 'startProcess' pin";
        return false;
    }

    // Impulsion sur la sortie startProcess
    if (!mController->setDigitalOutput(pin, true)) {
        mLastError = "ProcessZone::startProcess: failed to assert pin";
        return false;
    }

    // Maintenir le signal 100 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!mController->setDigitalOutput(pin, false)) {
        mLastError = "ProcessZone::startProcess: failed to deassert pin";
        return false;
    }

    return true;
}

bool ProcessZone::waitProcessDone(int timeoutMs)
{
    if (mController == nullptr) {
        mLastError = "ProcessZone::waitProcessDone: no controller";
        return false;
    }

    int donePin = getIoPin("done");
    int errorPin = getIoPin("error");

    if (donePin < 0) {
        mLastError = "ProcessZone::waitProcessDone: no 'done' pin";
        return false;
    }

    int elapsed = 0;
    const int pollMs = 50;

    while (elapsed < timeoutMs)
    {
        // Vérifier le signal "done"
        if (mController->waitDigitalInput(donePin, true, 0))
            return true;

        // Vérifier le signal "error"
        if (errorPin >= 0 && mController->waitDigitalInput(errorPin, true, 0))
        {
            mLastError = "ProcessZone: error signal received";
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
        elapsed += pollMs;
    }

    mLastError = "ProcessZone::waitProcessDone: timeout";
    return false;
}

bool ProcessZone::hasError()
{
    if (mController == nullptr) return false;

    int pin = getIoPin("error");
    if (pin < 0) return false;

    return mController->waitDigitalInput(pin, true, 0);
}

bool ProcessZone::resetError()
{
    // Les erreurs de process zone sont gérées par le protocole hardware.
    // Pour réarmer, on peut faire un cycle startProcess sans wafer
    // ou couper/rétablir l'alimentation. Ici, on ne fait rien.
    mLastError.clear();
    return true;
}

void ProcessZone::registerInLua(sol::state& lua)
{
    lua.new_usertype<ProcessZone>("ProcessZone",
        sol::constructors<ProcessZone()>(),
        "model",           sol::readonly(&ProcessZone::model),
        "manufacturer",    sol::readonly(&ProcessZone::manufacturer),
        "loadFromFile",    &ProcessZone::loadFromFile,
        "isReady",         &ProcessZone::isReady,
        "startProcess",    &ProcessZone::startProcess,
        "waitProcessDone", &ProcessZone::waitProcessDone,
        "hasError",        &ProcessZone::hasError,
        "resetError",      &ProcessZone::resetError,
        "lastError",       &ProcessZone::lastError,
        "dump",            &ProcessZone::dump
    );
}
