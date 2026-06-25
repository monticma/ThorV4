#pragma once
#include <string>
#include <vector>
#include "Component.h"

class Controller;
namespace sol
{
    class state;
}

struct EndEffectorUnits
{
    std::string length, angle, pressure, force, mass, time;
};

struct EndEffectorGeometry
{
    double innerRadius = 0.0, outerRadius = 0.0, gapAngle = 0.0, thickness = 0.0;
    std::string description;
};

struct EndEffectorVacuum
{
    int channels = 0;
    double holdingForce = 0.0;
    int settleTime = 0;
    double threshold = 0.0;
    std::string description;
};

struct EndEffectorWaferCompat
{
    double maxWeight = 0.0;
    std::vector<int> diameters;
    std::string description;
};

struct EndEffectorScanner
{
    bool mountable = false;
    std::string mountPosition, scannerType, description;
};

struct EndEffectorPhysical
{
    double weight = 0.0;
    std::string material, description;
};

struct EndEffectorDimensions
{
    double length = 0.0, width = 0.0, height = 0.0;
    std::string unit, description;
};

struct EndEffectorPayload
{
    double weight = 0.0;
    std::string weightUnit, description;
};

struct EndEffectorVariant
{
    std::string model, style;
    double weight = 0.0;
    std::string weightUnit;
    std::vector<int> waferDiameters;
    double innerRadius = 0.0, outerRadius = 0.0, width = 0.0, length = 0.0, thickness = 0.0;
    std::string description;
};

struct EndEffectorMetadata
{
    std::vector<EndEffectorVariant> variants;
    std::vector<std::string> notes;
};

class EndEffector : public Component
{
public:
    EndEffector() = default;
    EndEffector(const std::string &filePath);
    bool loadFromFile(const std::string &filePath) override;
    void dump() const override;
    std::string model, manufacturer, type, style, description;
    EndEffectorUnits units;
    EndEffectorGeometry geometry;
    EndEffectorVacuum vacuum;
    EndEffectorWaferCompat waferCompatibility;
    EndEffectorScanner scanner;
    EndEffectorPhysical physical;
    EndEffectorDimensions dimensions;
    EndEffectorPayload payload;
    std::vector<IoPinRequired> ioPinsRequired, ioPinsRequiredScanner;
    EndEffectorMetadata metadata;

    // Primitives
    bool pick(int settleMs);
    bool place(int settleMs);
    bool isWaferPresent();

    void setController(Controller* controller) override;
    void registerInLua(sol::state& lua) override;

private:
    Controller* mController = nullptr;
    int getVacuumOutputPin() const;
    int getVacuumSensorPin() const;
};
