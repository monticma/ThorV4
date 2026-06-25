#pragma once

#include <string>
#include <vector>

#include "Component.h"

#include <sol/forward.hpp>

class Controller;

struct TrackMechanical
{
    std::string guideType, driveType;
    double pitch = 0.0;
    std::string description;
};
struct TrackPhysical
{
    double length = 0.0, width = 0.0, height = 0.0, weight = 0.0;
    std::string weightUnit, mountingPattern, description;
};
struct TrackUnits
{
    std::string length, linearVelocity, linearAcceleration;
};
struct TrackVariant
{
    std::string model;
    double travel = 0.0;
    std::string description;
};
struct TrackMetadata
{
    std::vector<TrackVariant> variants;
    std::vector<std::string> notes;
};

class Track : public Component
{
public:
    Track() = default;
    Track(const std::string &filePath);
    bool loadFromFile(const std::string &filePath) override;
    void dump() const override;
    std::string model, manufacturer, description;
    int axisCount = 0;
    TrackUnits units;
    TrackMechanical mechanical;
    std::vector<ComponentAxis> axes;
    ComponentHoming homing;
    std::string visual;
    TrackPhysical physical;
    std::vector<IoPinRequired> ioPinsRequired;
    TrackMetadata metadata;

    bool moveTo(double position, double speed);
    bool moveRelative(double delta, double speed);
    bool home();
    bool stopMotion();
    bool emergencyStop();
    bool waitMotionDone(int timeoutMs);

    void setController(Controller* controller) override;
    void registerInLua(sol::state& lua) override;

private:
    Controller* mController = nullptr;
};
