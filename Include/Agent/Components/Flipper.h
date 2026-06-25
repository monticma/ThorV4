#pragma once
#include <string>
#include <vector>
#include "Component.h"

class Controller;
namespace sol
{
    class state;
}

struct FlipperUnits
{
    std::string angle, angularVelocity, angularAcceleration;
};
struct FlipperVacuum
{
    bool topChuck = true, bottomChuck = true;
    std::string description;
};
struct FlipperPhysical
{
    double width = 0.0, depth = 0.0, height = 0.0, weight = 0.0;
    std::string weightUnit, description;
};
struct FlipperMetadata
{
    std::vector<std::string> notes;
};

class Flipper : public Component
{
public:
    Flipper() = default;
    Flipper(const std::string &filePath);
    bool loadFromFile(const std::string &filePath) override;
    void dump() const override;
    std::string model, manufacturer, description;
    int axisCount = 0;
    FlipperUnits units;
    FlipperVacuum vacuum;
    FlipperPhysical physical;
    std::vector<ComponentAxis> axes;
    ComponentHoming homing;
    std::string visual;
    std::vector<IoPinRequired> ioPinsRequired;
    FlipperMetadata metadata;

    // Primitives
    bool flip(double speed);
    bool home();
    bool stopMotion();
    bool emergencyStop();

    void setController(Controller* controller) override;
    void registerInLua(sol::state& lua) override;

private:
    Controller* mController = nullptr;
};
