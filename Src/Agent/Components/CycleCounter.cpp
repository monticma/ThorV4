#include <fstream>

#include <nlohmann/json.hpp>

#include "Agent/Components/CycleCounter.h"

using json = nlohmann::json;

CycleCounter::CycleCounter(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool CycleCounter::loadFromFile(const std::string& filePath)
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

    description = j.value("description", "");

    if (j.contains("params") && j["params"].contains("reactions")) {
        for (auto& [key, val] : j["params"]["reactions"].items())
            params.reactions[key] = val.get<std::string>();
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
void CycleCounter::dump() const
{
    Component::dump();

    std::cout << "  [CycleCounter]\n";
    dumpField("description", description);

    if (!params.reactions.empty()) {
        std::cout << "    params.reactions (" << params.reactions.size() << ") :\n";
        for (const auto& [key, val] : params.reactions)
            std::cout << "      - " << key << " -> " << val << "\n";
    }

    dumpIoPins(ioPinsRequired);

    dumpStringVector("metadata.notes", metadata.notes, 4);
}
