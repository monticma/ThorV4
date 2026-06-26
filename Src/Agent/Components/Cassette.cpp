#include <fstream>
#include <thread>
#include <chrono>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/Cassette.h"
#include "Agent/Components/Controller.h"

using json = nlohmann::json;

Cassette::Cassette(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool Cassette::loadFromFile(const std::string& filePath)
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

    model       = j.value("model", "");
    standard    = j.value("standard", "");
    description = j.value("description", "");

    if (j.contains("units")) {
        auto& u = j["units"];
        units.length = u.value("length", "");
        units.time   = u.value("time", "");
    }

    if (j.contains("geometry")) {
        auto& g = j["geometry"];
        geometry.slotCount      = g.value("slotCount", 25);
        geometry.pitch          = g.value("pitch", 6.35);
        geometry.offset         = g.value("offset", 3.175);
        geometry.stroke         = g.value("stroke", 5.0);
        geometry.slotHeight     = g.value("slotHeight", 0.8);
        geometry.waferDiameter  = g.value("waferDiameter", 200);
        geometry.waferThickness = g.value("waferThickness", 0.725);
        geometry.description    = g.value("description", "");
    }

    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.width         = p.value("width", 0.0);
        physical.depth         = p.value("depth", 0.0);
        physical.height        = p.value("height", 0.0);
        physical.openingWidth  = p.value("openingWidth", 0.0);
        physical.openingHeight = p.value("openingHeight", 0.0);
        physical.weight        = p.value("weight", 0.0);
        physical.weightUnit    = p.value("weightUnit", "");
        physical.description   = p.value("description", "");
    }

    if (j.contains("waferSpecs")) {
        auto& w = j["waferSpecs"];
        waferSpecs.diameter    = w.value("diameter", 200);
        waferSpecs.description = w.value("description", "");
        if (w.contains("thickness")) {
            auto& t = w["thickness"];
            waferSpecs.thickness.min = t.value("min", 0.3);
            waferSpecs.thickness.max = t.value("max", 1.2);
        }
        if (w.contains("material")) {
            waferSpecs.material.clear();
            for (const auto& m : w["material"])
                waferSpecs.material.push_back(m.get<std::string>());
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
                CassetteVariant cv;
                cv.model        = v.value("model", "");
                cv.waferDiameter = v.value("waferDiameter", 200);
                cv.pitch        = v.value("pitch", 6.35);
                cv.slotCount    = v.value("slotCount", 25);
                cv.standard     = v.value("standard", "");
                cv.description  = v.value("description", "");
                metadata.variants.push_back(cv);
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
void Cassette::dump() const
{
    Component::dump();

    std::cout << "  [Cassette]\n";
    dumpField("model", model);
    dumpField("standard", standard);
    dumpField("description", description);

    std::cout << "    units:\n";
    dumpField("length", units.length, 6);
    dumpField("time", units.time, 6);

    std::cout << "    geometry:\n";
    dumpFieldInt("slotCount", geometry.slotCount, 6);
    dumpFieldDouble("pitch", geometry.pitch, 6);
    dumpFieldDouble("offset", geometry.offset, 6);
    dumpFieldDouble("stroke", geometry.stroke, 6);
    dumpFieldDouble("slotHeight", geometry.slotHeight, 6);
    dumpFieldInt("waferDiameter", geometry.waferDiameter, 6);
    dumpFieldDouble("waferThickness", geometry.waferThickness, 6);
    dumpField("description", geometry.description, 6);

    std::cout << "    physical:\n";
    dumpFieldDouble("width", physical.width, 6);
    dumpFieldDouble("depth", physical.depth, 6);
    dumpFieldDouble("height", physical.height, 6);
    dumpFieldDouble("openingWidth", physical.openingWidth, 6);
    dumpFieldDouble("openingHeight", physical.openingHeight, 6);
    dumpFieldDouble("weight", physical.weight, 6);
    dumpField("weightUnit", physical.weightUnit, 6);
    dumpField("description", physical.description, 6);

    std::cout << "    waferSpecs:\n";
    dumpFieldInt("diameter", waferSpecs.diameter, 6);
    std::cout << "      thickness: min=" << waferSpecs.thickness.min
              << " max=" << waferSpecs.thickness.max << "\n";
    dumpStringVector("material", waferSpecs.material, 6);
    dumpField("description", waferSpecs.description, 6);

    dumpIoPins(ioPinsRequired);

    if (!metadata.variants.empty()) {
        std::cout << "    metadata.variants (" << metadata.variants.size() << ") :\n";
        for (const auto& v : metadata.variants) {
            std::cout << "      - model=" << v.model
                      << " waferDiameter=" << v.waferDiameter
                      << " pitch=" << v.pitch
                      << " slotCount=" << v.slotCount
                      << " standard=" << v.standard;
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

void Cassette::setController(Controller* controller)
{
    mController = controller;
}

int Cassette::getIoPin(const std::string& idPattern) const
{
    for (size_t i = 0; i < ioPinsRequired.size(); ++i)
    {
        if (ioPinsRequired[i].id.find(idPattern) != std::string::npos)
            return static_cast<int>(i);
    }
    return -1;
}

bool Cassette::isPresent()
{
    if (mController == nullptr) {
        mLastError = "Cassette::isPresent: no controller";
        return false;
    }

    int pin = getIoPin("cassettePresence");
    if (pin < 0) {
        mLastError = "Cassette::isPresent: no presence sensor configured";
        return false;
    }

    // waitDigitalInput avec timeout 0 = lecture immédiate
    return mController->waitDigitalInput(pin, true, 0);
}

int Cassette::getSlotCount() const
{
    return geometry.slotCount;
}

bool Cassette::getSlotStatus(int slotNumber, bool& waferPresentOut)
{
    if (slotNumber < 1 || slotNumber > geometry.slotCount) {
        mLastError = "Cassette::getSlotStatus: invalid slot " +
                     std::to_string(slotNumber);
        return false;
    }

    if (slotNumber <= static_cast<int>(mSlotStatus.size())) {
        waferPresentOut = mSlotStatus[slotNumber - 1];
        return true;
    }

    waferPresentOut = false;
    return true;
}

bool Cassette::isSlotOccupied(int slotNumber)
{
    bool occupied = false;
    getSlotStatus(slotNumber, occupied);
    return occupied;
}

bool Cassette::scanSlots(int fromSlot, int toSlot,
                          std::vector<bool>& resultsOut)
{
    resultsOut.clear();

    if (mController == nullptr) {
        mLastError = "Cassette::scanSlots: no controller";
        return false;
    }

    if (fromSlot < 1) fromSlot = 1;
    if (toSlot > geometry.slotCount) toSlot = geometry.slotCount;
    if (fromSlot > toSlot) {
        mLastError = "Cassette::scanSlots: fromSlot > toSlot";
        return false;
    }

    int count = toSlot - fromSlot + 1;
    resultsOut.resize(count, false);

    // Le scan réel nécessite le laser (beamBreak sensor) et le mouvement Z.
    // Pour l'instant, on simule : tous les slots sont vides.
    // L'implémentation complète sera faite quand le scanner laser sera
    // intégré avec le mouvement du robot.
    for (int i = 0; i < count; ++i) {
        resultsOut[i] = false;
    }

    // Mettre à jour le cache
    mSlotStatus.resize(geometry.slotCount, false);
    for (int i = 0; i < count; ++i) {
        int slotIdx = fromSlot + i - 1;
        if (slotIdx < static_cast<int>(mSlotStatus.size()))
            mSlotStatus[slotIdx] = resultsOut[i];
    }

    return true;
}

void Cassette::registerInLua(sol::state& lua)
{
    lua.new_usertype<Cassette>("Cassette",
        sol::constructors<Cassette()>(),
        "model",          sol::readonly(&Cassette::model),
        "standard",       sol::readonly(&Cassette::standard),
        "loadFromFile",   &Cassette::loadFromFile,
        "isPresent",      &Cassette::isPresent,
        "getSlotCount",   &Cassette::getSlotCount,
        "isSlotOccupied", &Cassette::isSlotOccupied,
        "scanSlots",      &Cassette::scanSlots,
        "lastError",      &Cassette::lastError,
        "dump",           &Cassette::dump
    );
}
