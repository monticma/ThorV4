#include <fstream>

#include <nlohmann/json.hpp>

#include "Agent/Components/ProcessZone.h"

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
