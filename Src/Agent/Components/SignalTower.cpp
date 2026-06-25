#include <fstream>
#include <cctype>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/SignalTower.h"
#include "Agent/Components/Controller.h"

using json = nlohmann::json;

SignalTower::SignalTower(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool SignalTower::loadFromFile(const std::string& filePath)
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

    // --- lights ---
    if (j.contains("lights")) {
        auto& l = j["lights"];
        lights.count       = l.value("count", 3);
        lights.description = l.value("description", "");
        if (l.contains("colors")) {
            lights.colors.clear();
            for (const auto& c : l["colors"])
                lights.colors.push_back(c.get<std::string>());
        }
        if (l.contains("modes")) {
            lights.modes.clear();
            for (const auto& m : l["modes"])
                lights.modes.push_back(m.get<std::string>());
        }
    }

    // --- buzzer ---
    if (j.contains("buzzer")) {
        auto& b = j["buzzer"];
        buzzer.available   = b.value("available", true);
        buzzer.description = b.value("description", "");
        if (b.contains("modes")) {
            buzzer.modes.clear();
            for (const auto& m : b["modes"])
                buzzer.modes.push_back(m.get<std::string>());
        }
    }

    // --- defaults ---
    if (j.contains("defaults")) {
        auto& d = j["defaults"];
        defaults.blinkRateHz       = d.value("blinkRateHz", 2);
        defaults.buzzerFrequencyHz = d.value("buzzerFrequencyHz", 2800);
        defaults.initialPattern    = d.value("initialPattern", "off");
    }

    // --- patterns ---
    if (j.contains("patterns")) {
        for (auto& [key, val] : j["patterns"].items()) {
            SignalTowerPattern pat;
            pat.green  = val.value("green", "off");
            pat.yellow = val.value("yellow", "off");
            pat.red    = val.value("red", "off");
            pat.blue   = val.value("blue", "off");
            pat.white  = val.value("white", "off");
            pat.buzzer = val.value("buzzer", "off");
            patterns[key] = pat;
        }
    }

    // --- reactions ---
    if (j.contains("reactions")) {
        for (auto& [key, val] : j["reactions"].items())
            reactions[key] = val.get<std::string>();
    }

    // --- physical ---
    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.height          = p.value("height", 0.0);
        physical.diameter        = p.value("diameter", 0.0);
        physical.weight          = p.value("weight", 0.0);
        physical.weightUnit      = p.value("weightUnit", "");
        physical.voltage         = p.value("voltage", "");
        physical.mountingType    = p.value("mountingType", "");
        physical.protectionRating = p.value("protectionRating", "");
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
                SignalTowerVariant sv;
                sv.model       = v.value("model", "");
                sv.lightCount  = v.value("lightCount", 3);
                sv.description = v.value("description", "");
                if (v.contains("colors")) {
                    for (const auto& c : v["colors"])
                        sv.colors.push_back(c.get<std::string>());
                }
                metadata.variants.push_back(sv);
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
void SignalTower::dump() const
{
    Component::dump();

    std::cout << "  [SignalTower]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("description", description);

    std::cout << "    lights:\n";
    dumpFieldInt("count", lights.count, 6);
    dumpStringVector("colors", lights.colors, 6);
    dumpStringVector("modes", lights.modes, 6);
    dumpField("description", lights.description, 6);

    std::cout << "    buzzer:\n";
    dumpFieldBool("available", buzzer.available, 6);
    dumpStringVector("modes", buzzer.modes, 6);
    dumpField("description", buzzer.description, 6);

    std::cout << "    defaults:\n";
    dumpFieldInt("blinkRateHz", defaults.blinkRateHz, 6);
    dumpFieldInt("buzzerFrequencyHz", defaults.buzzerFrequencyHz, 6);
    dumpField("initialPattern", defaults.initialPattern, 6);

    if (!patterns.empty()) {
        std::cout << "    patterns (" << patterns.size() << ") :\n";
        for (const auto& [key, pat] : patterns) {
            std::cout << "      - " << key
                      << " green=" << pat.green
                      << " yellow=" << pat.yellow
                      << " red=" << pat.red
                      << " blue=" << pat.blue
                      << " white=" << pat.white
                      << " buzzer=" << pat.buzzer << "\n";
        }
    }

    if (!reactions.empty()) {
        std::cout << "    reactions (" << reactions.size() << ") :\n";
        for (const auto& [key, val] : reactions)
            std::cout << "      - " << key << " -> " << val << "\n";
    }

    std::cout << "    physical:\n";
    dumpFieldDouble("height", physical.height, 6);
    dumpFieldDouble("diameter", physical.diameter, 6);
    dumpFieldDouble("weight", physical.weight, 6);
    dumpField("weightUnit", physical.weightUnit, 6);
    dumpField("voltage", physical.voltage, 6);
    dumpField("mountingType", physical.mountingType, 6);
    dumpField("protectionRating", physical.protectionRating, 6);
    dumpField("description", physical.description, 6);

    dumpIoPins(ioPinsRequired);

    if (!metadata.variants.empty()) {
        std::cout << "    metadata.variants (" << metadata.variants.size() << ") :\n";
        for (const auto& v : metadata.variants) {
            std::cout << "      - model=" << v.model
                      << " lightCount=" << v.lightCount;
            if (!v.colors.empty()) {
                std::cout << " colors=[";
                for (size_t i = 0; i < v.colors.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << v.colors[i];
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

void SignalTower::setController(Controller* controller)
{
    mController = controller;
}

int SignalTower::getPinForColor(const std::string& color) const
{
    for (size_t i = 0; i < ioPinsRequired.size(); ++i)
    {
        const auto& io = ioPinsRequired[i];
        std::string idLower = io.id;
        std::string colorLower = color;
        for (auto& c : idLower) c = static_cast<char>(std::tolower(c));
        for (auto& c : colorLower) c = static_cast<char>(std::tolower(c));

        if (idLower.find(colorLower) != std::string::npos)
            return static_cast<int>(i);
    }
    return -1;
}

bool SignalTower::setLight(const std::string& color, const std::string& mode)
{
    if (mController == nullptr) {
        mLastError = "SignalTower::setLight: no controller";
        return false;
    }

    int pin = getPinForColor(color);
    if (pin < 0) {
        mLastError = "SignalTower::setLight: no pin for color '" + color + "'";
        return false;
    }

    bool value = (mode == "on" || mode == "blink");

    if (!mController->setDigitalOutput(pin, value)) {
        mLastError = "SignalTower::setLight: failed to set pin "
                     + std::to_string(pin);
        return false;
    }

    return true;
}

bool SignalTower::setBuzzer(const std::string& mode)
{
    if (mController == nullptr) {
        mLastError = "SignalTower::setBuzzer: no controller";
        return false;
    }

    // Chercher la pin du buzzer
    for (size_t i = 0; i < ioPinsRequired.size(); ++i)
    {
        const auto& io = ioPinsRequired[i];
        if (io.id.find("buzzer") != std::string::npos
            || io.id.find("Buzzer") != std::string::npos)
        {
            bool value = (mode == "on" || mode == "pulse");
            return mController->setDigitalOutput(static_cast<int>(i), value);
        }
    }

    mLastError = "SignalTower::setBuzzer: no buzzer pin configured";
    return false;
}

bool SignalTower::applyPattern(const std::string& patternName)
{
    auto it = patterns.find(patternName);
    if (it == patterns.end()) {
        mLastError = "SignalTower::applyPattern: unknown pattern '" + patternName + "'";
        return false;
    }

    const auto& p = it->second;
    bool ok = true;

    if (!p.green.empty())  ok = setLight("green", p.green) && ok;
    if (!p.yellow.empty()) ok = setLight("yellow", p.yellow) && ok;
    if (!p.red.empty())    ok = setLight("red", p.red) && ok;
    if (!p.buzzer.empty()) ok = setBuzzer(p.buzzer) && ok;

    return ok;
}

void SignalTower::registerInLua(sol::state& lua)
{
    lua.new_usertype<SignalTower>("SignalTower",
        sol::constructors<SignalTower()>(),
        "model",         sol::readonly(&SignalTower::model),
        "manufacturer",  sol::readonly(&SignalTower::manufacturer),
        "loadFromFile",  &SignalTower::loadFromFile,
        "setLight",      &SignalTower::setLight,
        "setBuzzer",     &SignalTower::setBuzzer,
        "applyPattern",  &SignalTower::applyPattern,
        "lastError",     &SignalTower::lastError,
        "dump",          &SignalTower::dump
    );
}
