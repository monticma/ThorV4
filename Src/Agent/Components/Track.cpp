#include <fstream>

#include <nlohmann/json.hpp>

#include "Agent/Components/Track.h"

using json = nlohmann::json;

Track::Track(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool Track::loadFromFile(const std::string& filePath)
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
        units.length             = u.value("length", "");
        units.linearVelocity     = u.value("linearVelocity", "");
        units.linearAcceleration = u.value("linearAcceleration", "");
    }

    // --- Racine ---
    axisCount = j.value("axisCount", 0);

    // --- axes (format standardisé) ---
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

    // --- visual ---
    visual = j.value("visual", "");

    // --- mechanical ---
    if (j.contains("mechanical")) {
        auto& m = j["mechanical"];
        mechanical.guideType   = m.value("guideType", "");
        mechanical.driveType   = m.value("driveType", "");
        mechanical.pitch       = m.value("pitch", 0.0);
        mechanical.description = m.value("description", "");
    }

    // --- physical ---
    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.length          = p.value("length", 0.0);
        physical.width           = p.value("width", 0.0);
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
                TrackVariant tv;
                tv.model       = v.value("model", "");
                tv.travel      = v.value("travel", 0.0);
                tv.description = v.value("description", "");
                metadata.variants.push_back(tv);
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
void Track::dump() const
{
    Component::dump();

    std::cout << "  [Track]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("description", description);

    std::cout << "    units:\n";
    dumpField("length", units.length, 6);
    dumpField("linearVelocity", units.linearVelocity, 6);
    dumpField("linearAcceleration", units.linearAcceleration, 6);

    dumpFieldInt("axisCount", axisCount);
    dumpAxes(axes);
    dumpHoming(homing);
    dumpField("visual", visual);

    std::cout << "    mechanical:\n";
    dumpField("guideType", mechanical.guideType, 6);
    dumpField("driveType", mechanical.driveType, 6);
    dumpFieldDouble("pitch", mechanical.pitch, 6);
    dumpField("description", mechanical.description, 6);

    std::cout << "    physical:\n";
    dumpFieldDouble("length", physical.length, 6);
    dumpFieldDouble("width", physical.width, 6);
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
                      << " travel=" << v.travel;
            if (!v.description.empty())
                std::cout << " (" << v.description << ")";
            std::cout << "\n";
        }
    }
    dumpStringVector("metadata.notes", metadata.notes, 4);
}
