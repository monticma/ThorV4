#include <fstream>

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include "Agent/Components/Robot.h"
#include "Agent/Components/Controller.h"
#include "Agent/Kinematics/DHKinematics.h"
#include "Agent/Drivers/GalilDriver.h"

using json = nlohmann::json;

Robot::Robot(const std::string& filePath)
{
    loadFromFile(filePath);
}

bool Robot::loadFromFile(const std::string& filePath)
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
    }

    // --- kinematics ---
    if (j.contains("kinematics")) {
        auto& k = j["kinematics"];
        kinematics.type             = k.value("type", "");
        kinematics.coordinateSystem = k.value("coordinateSystem", "");
        kinematics.visualizerType   = k.value("visualizerType", "");
        kinematics.pluginLibrary    = k.value("pluginLibrary", "");
        kinematics.pluginConfig     = k.value("pluginConfig", "");

        if (k.contains("workspace")) {
            auto& w = k["workspace"];
            kinematics.workspace.minReach = w.value("minReach", 0.0);
            kinematics.workspace.maxReach = w.value("maxReach", 0.0);
            kinematics.workspace.minZ    = w.value("minZ", 0.0);
            kinematics.workspace.maxZ    = w.value("maxZ", 0.0);
        }

        if (k.contains("dhParameters")) {
            kinematics.dhParameters.clear();
            for (const auto& dh : k["dhParameters"]) {
                DHParameter dp;
                dp.a           = dh.value("a", 0.0);
                dp.alpha       = dh.value("alpha", 0.0);

                // "d" peut être un nombre ou un string ("q2")
                if (dh["d"].is_number()) {
                    dp.d = dh["d"].get<double>();
                } else if (dh["d"].is_string()) {
                    dp.dVar = dh["d"].get<std::string>();
                }

                // "theta" peut être un nombre ou un string ("q0", "q1")
                if (dh["theta"].is_number()) {
                    // stocké comme string quand même pour uniformité
                    dp.theta = std::to_string(dh["theta"].get<double>());
                } else if (dh["theta"].is_string()) {
                    dp.theta = dh["theta"].get<std::string>();
                }

                dp.type        = dh.value("type", "");
                dp.description = dh.value("description", "");
                kinematics.dhParameters.push_back(dp);
            }
        }
    }

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

    // --- capabilities ---
    if (j.contains("capabilities")) {
        auto& c = j["capabilities"];
        if (c.contains("primitives")) {
            capabilities.primitives.clear();
            for (const auto& v : c["primitives"])
                capabilities.primitives.push_back(v.get<std::string>());
        }
        if (c.contains("features")) {
            capabilities.features.clear();
            for (const auto& v : c["features"])
                capabilities.features.push_back(v.get<std::string>());
        }
    }

    // --- visual ---
    visual = j.value("visual", "");

    // --- physical ---
    if (j.contains("physical")) {
        auto& p = j["physical"];
        physical.reach       = p.value("reach", 0.0);
        physical.zTravel     = p.value("zTravel", 0.0);
        physical.weight      = p.value("weight", 0.0);
        physical.weightUnit  = p.value("weightUnit", "");
        physical.mountType   = p.value("mountType", "");
        physical.environment = p.value("environment", "");
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

    // --- metadata ---
    if (j.contains("metadata")) {
        auto& m = j["metadata"];
        if (m.contains("variants")) {
            metadata.variants.clear();
            for (const auto& v : m["variants"]) {
                RobotVariant rv;
                rv.model       = v.value("model", "");
                rv.reach       = v.value("reach", 0.0);
                rv.zTravel     = v.value("zTravel", 0.0);
                rv.description = v.value("description", "");
                metadata.variants.push_back(rv);
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
void Robot::dump() const
{
    Component::dump();

    std::cout << "  [Robot]\n";
    dumpField("model", model);
    dumpField("manufacturer", manufacturer);
    dumpField("description", description);

    std::cout << "    units:\n";
    dumpField("length", units.length, 6);
    dumpField("angle", units.angle, 6);
    dumpField("linearVelocity", units.linearVelocity, 6);
    dumpField("angularVelocity", units.angularVelocity, 6);
    dumpField("linearAcceleration", units.linearAcceleration, 6);
    dumpField("angularAcceleration", units.angularAcceleration, 6);

    std::cout << "    kinematics:\n";
    dumpField("type", kinematics.type, 6);
    dumpField("coordinateSystem", kinematics.coordinateSystem, 6);
    dumpField("visualizerType", kinematics.visualizerType, 6);
    dumpField("pluginLibrary", kinematics.pluginLibrary, 6);
    dumpField("pluginConfig", kinematics.pluginConfig, 6);
    std::cout << "      workspace: minReach=" << kinematics.workspace.minReach
              << " maxReach=" << kinematics.workspace.maxReach
              << " minZ=" << kinematics.workspace.minZ
              << " maxZ=" << kinematics.workspace.maxZ << "\n";
    if (!kinematics.dhParameters.empty()) {
        std::cout << "      dhParameters (" << kinematics.dhParameters.size() << ") :\n";
        for (const auto& dh : kinematics.dhParameters) {
            std::cout << "        - a=" << dh.a << " alpha=" << dh.alpha
                      << " d=" << dh.d << " dVar=" << dh.dVar
                      << " theta=" << dh.theta
                      << " type=" << dh.type;
            if (!dh.description.empty())
                std::cout << " (" << dh.description << ")";
            std::cout << "\n";
        }
    }

    dumpAxes(axes);
    dumpHoming(homing);

    std::cout << "    capabilities:\n";
    dumpStringVector("primitives", capabilities.primitives, 6);
    dumpStringVector("features", capabilities.features, 6);

    dumpField("visual", visual);

    std::cout << "    physical:\n";
    dumpFieldDouble("reach", physical.reach, 6);
    dumpFieldDouble("zTravel", physical.zTravel, 6);
    dumpFieldDouble("weight", physical.weight, 6);
    dumpField("weightUnit", physical.weightUnit, 6);
    dumpField("mountType", physical.mountType, 6);
    dumpField("environment", physical.environment, 6);
    dumpField("description", physical.description, 6);

    dumpIoPins(ioPinsRequired);

    if (!metadata.variants.empty()) {
        std::cout << "    metadata.variants (" << metadata.variants.size() << ") :\n";
        for (const auto& v : metadata.variants) {
            std::cout << "      - model=" << v.model
                      << " reach=" << v.reach
                      << " zTravel=" << v.zTravel;
            if (!v.description.empty())
                std::cout << " (" << v.description << ")";
            std::cout << "\n";
        }
    }
    dumpStringVector("metadata.notes", metadata.notes, 4);
}

// =============================================================================
// Primitives (nouveau)
// =============================================================================

void Robot::setController(Controller* controller)
{
    mController = controller;

    // Initialiser le solveur cinématique après chargement des DH
    if (!kinematics.dhParameters.empty()) {
        mKinematics = new DHKinematics();
        mKinematics->loadFromDhParams(kinematics.dhParameters);
    }
}

bool Robot::moveTo(double x, double y, double z, double speed)
{
    if (mController == nullptr) {
        mLastError = "Robot::moveTo: no controller";
        return false;
    }
    if (mKinematics == nullptr) {
        mLastError = "Robot::moveTo: no kinematics solver";
        return false;
    }

    // IK : position cartésienne → angles joints
    std::vector<double> jointAngles;
    if (!mKinematics->inverseKinematics(x, y, z, jointAngles)) {
        mLastError = mKinematics->lastError();
        return false;
    }

    // Conversion angles → counts
    std::vector<double> counts;
    if (!jointAnglesToCounts(jointAngles, counts)) {
        return false;
    }

    // Appliquer le facteur de vitesse (%)
    double speedFactor = speed / 100.0;
    if (speedFactor < 0.01) speedFactor = 0.01;
    if (speedFactor > 1.0)  speedFactor = 1.0;

    // Calculer les vitesses par axe
    std::vector<double> speedCounts;
    for (size_t i = 0; i < axes.size() && i < 8; ++i) {
        speedCounts.push_back(axes[i].maxVelocity * speedFactor);
    }
    // Compléter à 8 axes pour le Galil
    while (speedCounts.size() < 8) speedCounts.push_back(0.0);

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Robot::moveTo: driver not available";
        return false;
    }

    // Définir les vitesses
    driver->setSpeeds(speedCounts);

    // Mouvement absolu
    return driver->moveAbsolute(counts);
}

bool Robot::moveJoint(int jointIndex, double angle, double speed)
{
    if (mController == nullptr) {
        mLastError = "Robot::moveJoint: no controller";
        return false;
    }
    if (jointIndex < 0 || jointIndex >= static_cast<int>(axes.size())) {
        mLastError = "Robot::moveJoint: invalid joint index";
        return false;
    }

    // Lire les positions courantes
    std::vector<double> currentCounts;
    if (!readCurrentPositions(currentCounts)) {
        return false;
    }

    // Calculer les counts pour le joint cible
    double countsPerUnit = 1.0;
    // Utiliser le countsPerUnit de l'axis mapping si disponible
    // (sinon 1.0 par défaut)
    double targetCounts = angle * countsPerUnit;
    currentCounts[jointIndex] = targetCounts;

    // Vitesse
    double speedFactor = speed / 100.0;
    if (speedFactor < 0.01) speedFactor = 0.01;
    if (speedFactor > 1.0)  speedFactor = 1.0;

    std::vector<double> speedCounts(8, 0.0);
    speedCounts[jointIndex] = axes[jointIndex].maxVelocity * speedFactor;

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Robot::moveJoint: driver not available";
        return false;
    }

    driver->setSpeeds(speedCounts);
    return driver->moveAbsolute(currentCounts);
}

bool Robot::getPosition(std::vector<double>& positionOut)
{
    positionOut.clear();

    if (mController == nullptr || mKinematics == nullptr) {
        mLastError = "Robot::getPosition: not initialized";
        return false;
    }

    std::vector<double> counts;
    if (!readCurrentPositions(counts)) {
        return false;
    }

    std::vector<double> jointAngles;
    for (size_t i = 0; i < axes.size() && i < counts.size(); ++i) {
        jointAngles.push_back(counts[i]);
    }

    double x, y, z;
    mKinematics->forwardKinematics(jointAngles, x, y, z);

    positionOut.push_back(x);
    positionOut.push_back(y);
    positionOut.push_back(z);
    return true;
}

bool Robot::home()
{
    if (mController == nullptr) {
        mLastError = "Robot::home: no controller";
        return false;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Robot::home: driver not available";
        return false;
    }

    // Homing de tous les axes dans l'ordre défini
    for (size_t i = 0; i < homing.sequence.size(); ++i) {
        const std::string& axisName = homing.sequence[i];

        // Trouver l'index de l'axe
        int axisIndex = -1;
        for (size_t j = 0; j < axes.size(); ++j) {
            if (axes[j].name == axisName) {
                axisIndex = static_cast<int>(j);
                break;
            }
        }

        if (axisIndex < 0) {
            mLastError = "Robot::home: unknown axis '" + axisName + "'";
            return false;
        }

        if (!driver->home(axisIndex)) {
            mLastError = "Robot::home: failed for axis " + axisName
                         + ": " + driver->lastError();
            return false;
        }
    }

    return true;
}

bool Robot::emergencyStop()
{
    if (mController == nullptr) {
        return false;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        return false;
    }

    driver->stopAll();
    driver->motorOff();
    return true;
}

void Robot::registerInLua(sol::state& lua)
{
    lua.new_usertype<Robot>("Robot",
        sol::constructors<Robot()>(),
        "model",         sol::readonly(&Robot::model),
        "manufacturer",  sol::readonly(&Robot::manufacturer),
        "loadFromFile",  &Robot::loadFromFile,
        "moveTo",        &Robot::moveTo,
        "moveJoint",     &Robot::moveJoint,
        "getPosition",   &Robot::getPosition,
        "home",          &Robot::home,
        "emergencyStop", &Robot::emergencyStop,
        "lastError",     &Robot::lastError,
        "dump",          &Robot::dump
    );
}

// =============================================================================
// Helpers privés
// =============================================================================

bool Robot::jointAnglesToCounts(const std::vector<double>& jointAngles,
                                 std::vector<double>& countsOut)
{
    countsOut.clear();

    for (size_t i = 0; i < axes.size() && i < jointAngles.size(); ++i) {
        countsOut.push_back(jointAngles[i]);
    }
    while (countsOut.size() < 8) {
        countsOut.push_back(0.0);
    }
    return true;
}

bool Robot::readCurrentPositions(std::vector<double>& countsOut)
{
    if (mController == nullptr) {
        return false;
    }
    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        return false;
    }
    return driver->getPositions(countsOut);
}
