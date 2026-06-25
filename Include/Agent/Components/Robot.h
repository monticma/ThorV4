#pragma once
#include <string>
#include <vector>
#include "Component.h"
class Controller; class DHKinematics; namespace sol { class state; }

struct DHParameter { double a = 0.0, alpha = 0.0, d = 0.0; std::string theta, dVar, type, description; };
struct RobotWorkspace { double minReach = 0.0, maxReach = 0.0, minZ = 0.0, maxZ = 0.0; };
struct RobotKinematics { std::string type, coordinateSystem, visualizerType, pluginLibrary, pluginConfig; RobotWorkspace workspace; std::vector<DHParameter> dhParameters; };
struct RobotCapabilities { std::vector<std::string> primitives, features; };
struct RobotPhysical { double reach = 0.0, zTravel = 0.0, weight = 0.0; std::string weightUnit, mountType, environment, description; };
struct RobotUnits { std::string length, angle, linearVelocity, angularVelocity, linearAcceleration, angularAcceleration; };
struct RobotVariant { std::string model; double reach = 0.0, zTravel = 0.0; std::string description; };
struct RobotMetadata { std::vector<RobotVariant> variants; std::vector<std::string> notes; };

class Robot : public Component {
public:
    Robot() = default;
    Robot(const std::string& filePath);
    bool loadFromFile(const std::string& filePath) override;
    void dump() const override;
    std::string model, manufacturer, description;
    RobotUnits units; RobotKinematics kinematics;
    std::vector<ComponentAxis> axes; ComponentHoming homing;
    RobotCapabilities capabilities; std::string visual;
    RobotPhysical physical; std::vector<IoPinRequired> ioPinsRequired; RobotMetadata metadata;

    void setController(Controller* controller) override;
    bool moveTo(double x, double y, double z, double speed);
    bool moveJoint(int jointIndex, double angle, double speed);
    bool moveRelative(int jointIndex, double delta, double speed);
    bool getPosition(std::vector<double>& positionOut);
    bool home(); bool stopMotion(); bool emergencyStop();
    bool waitMotionDone(int timeoutMs);
    bool pick(int axis, int port, double distance, int settleMs);
    bool place(int axis, int port, double distance, int settleMs);
    void registerInLua(sol::state& lua) override;
private:
    Controller* mController = nullptr;
    DHKinematics* mKinematics = nullptr;
    bool jointAnglesToCounts(const std::vector<double>& jointAngles, std::vector<double>& countsOut);
    bool readCurrentPositions(std::vector<double>& countsOut);
};
