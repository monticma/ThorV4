#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>

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
    if (!driver->setSpeeds(speedCounts)) {
        mLastError = "Robot::moveTo: setSpeeds failed: " + driver->lastError();
        return false;
    }

    // Mouvement absolu
    if (!driver->moveAbsolute(counts)) {
        mLastError = "Robot::moveTo: moveAbsolute failed: " + driver->lastError();
        return false;
    }

    return true;
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

    // Calculer la cible pour le joint (angle en degrés = unités utilisateur)
    // Le contrôleur applique son propre countsPerUnit
    double targetCounts = angle;
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

    if (!driver->setSpeeds(speedCounts)) {
        mLastError = "Robot::moveJoint: setSpeeds failed: " + driver->lastError();
        return false;
    }
    if (!driver->moveAbsolute(currentCounts)) {
        mLastError = "Robot::moveJoint: moveAbsolute failed: " + driver->lastError();
        return false;
    }
    return true;
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

    // Convertir les counts en angles joints (radians pour revolutes)
    // Le contrôleur (xmul/Galil) renvoie des unités utilisateur (degrés ou mm).
    std::vector<double> jointAngles;
    for (size_t i = 0; i < axes.size() && i < counts.size(); ++i)
    {
        double value = counts[i];

        // Le contrôleur renvoie déjà en unités utilisateur (degrés ou mm).
        // Pas de division par countsPerUnit — déjà fait par le contrôleur.

        // degrés → radians pour les axes rotatifs
        if (axes[i].type == "revolute")
            value = value * M_PI / 180.0;

        jointAngles.push_back(value);
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

bool Robot::moveRelative(int jointIndex, double delta, double speed)
{
    if (mController == nullptr) {
        mLastError = "Robot::moveRelative: no controller";
        return false;
    }
    if (jointIndex < 0 || jointIndex >= static_cast<int>(axes.size())) {
        mLastError = "Robot::moveRelative: invalid joint index";
        return false;
    }

    std::vector<double> deltas(8, 0.0);
    deltas[jointIndex] = delta;

    double speedFactor = speed / 100.0;
    if (speedFactor < 0.01) speedFactor = 0.01;
    if (speedFactor > 1.0)  speedFactor = 1.0;

    std::vector<double> speedCounts(8, 0.0);
    speedCounts[jointIndex] = axes[jointIndex].maxVelocity * speedFactor;

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Robot::moveRelative: driver not available";
        return false;
    }

    driver->setSpeeds(speedCounts);
    return driver->moveRelative(deltas);
}

bool Robot::stopMotion()
{
    if (mController == nullptr) return false;
    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) return false;
    return driver->stopAll();
}

bool Robot::waitMotionDone(int timeoutMs)
{
    if (mController == nullptr) {
        mLastError = "Robot::waitMotionDone: no controller";
        return false;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Robot::waitMotionDone: driver not available";
        return false;
    }

    std::vector<double> prevPositions;
    std::vector<double> curPositions;
    int elapsed = 0;
    const int pollIntervalMs = 50;

    if (!driver->getPositions(prevPositions)) return false;

    while (elapsed < timeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;

        if (!driver->getPositions(curPositions)) return false;

        bool stable = true;
        for (size_t i = 0; i < curPositions.size() && i < prevPositions.size(); ++i) {
            if (std::abs(curPositions[i] - prevPositions[i]) > 1.0) {
                stable = false;
                break;
            }
        }
        if (stable) return true;
        prevPositions = curPositions;
    }

    mLastError = "Robot::waitMotionDone: timeout";
    return false;
}

bool Robot::pick(int axis, int port, double distance, int settleMs)
{
    if (mController == nullptr) {
        mLastError = "Robot::pick: no controller";
        return false;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Robot::pick: driver not available";
        return false;
    }

    // 1. Activer le vacuum via I/O (l'end effector est sur le contrôleur)
    if (!driver->setDigitalOutput(port, true)) {
        mLastError = "Robot::pick: failed to activate vacuum port "
                     + std::to_string(port) + ": " + driver->lastError();
        return false;
    }

    // 2. Temps de stabilisation du vide
    std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));

    // 3. Mouvement de retrait (si distance > 0)
    if (distance > 0.0) {
        std::vector<double> deltas(8, 0.0);
        deltas[axis] = -distance; // retrait = direction opposée
        if (!driver->moveRelative(deltas)) {
            mLastError = "Robot::pick: retract failed: " + driver->lastError();
            return false;
        }
    }

    return true;
}

bool Robot::place(int axis, int port, double distance, int settleMs)
{
    if (mController == nullptr) {
        mLastError = "Robot::place: no controller";
        return false;
    }

    GalilDriver* driver = mController->getDriver();
    if (driver == nullptr) {
        mLastError = "Robot::place: driver not available";
        return false;
    }

    // 1. Mouvement d'approche (si distance > 0)
    if (distance > 0.0) {
        std::vector<double> deltas(8, 0.0);
        deltas[axis] = distance;
        if (!driver->moveRelative(deltas)) {
            mLastError = "Robot::place: approach failed: " + driver->lastError();
            return false;
        }
    }

    // 2. Temps de stabilisation
    std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));

    // 3. Désactiver le vacuum
    if (!driver->setDigitalOutput(port, false)) {
        mLastError = "Robot::place: failed to deactivate vacuum port "
                     + std::to_string(port) + ": " + driver->lastError();
        return false;
    }

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
        "moveRelative",  &Robot::moveRelative,
        "getPosition",   &Robot::getPositionLua,
        "home",          &Robot::home,
        "stopMotion",    &Robot::stopMotion,
        "emergencyStop", &Robot::emergencyStop,
        "waitMotionDone",&Robot::waitMotionDone,
        "pick",          &Robot::pick,
        "place",         &Robot::place,
        "lastError",     &Robot::lastError,
        "dump",          &Robot::dump
    );
}

// =============================================================================
// Wrapper Lua pour getPosition (retourne x, y, z au lieu de vector&)
// =============================================================================

std::tuple<double, double, double> Robot::getPositionLua()
{
    std::vector<double> pos;
    if (!getPosition(pos) || pos.size() < 3)
        return std::make_tuple(0.0, 0.0, 0.0);
    return std::make_tuple(pos[0], pos[1], pos[2]);
}

// =============================================================================
// Helpers privés
// =============================================================================

bool Robot::jointAnglesToCounts(const std::vector<double>& jointAngles,
                                 std::vector<double>& countsOut)
{
    countsOut.clear();

    for (size_t i = 0; i < axes.size() && i < jointAngles.size(); ++i)
    {
        // Les angles sont en radians (sortie IK).
        // Pour les axes rotatifs : convertir en degrés (unités utilisateur).
        // Pour les axes prismatiques : mm (unités utilisateur).
        // Le contrôleur (xmul ou Galil réel) applique son propre countsPerUnit.
        double value = jointAngles[i];

        if (axes[i].type == "revolute")
        {
            // radians → degrés
            value = value * 180.0 / M_PI;
        }
        // Pas de multiplication par countsPerUnit — le contrôleur le fait

        countsOut.push_back(value);
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
