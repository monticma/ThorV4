#pragma once
#include <string>
#include <vector>
#include "Component.h"

struct AlignerAlignment {
    std::vector<std::string> modes; bool autoDetect = true;
    std::vector<int> waferSizes; int defaultWaferSize = 200;
    std::string sensorType, lightSource; int settleTime = 300, maxAlignmentTime = 15000;
    double repeatability = 0.0; std::string repeatabilityUnit, description;
};
struct AlignerPinAssembly { int pinCount = 3; std::string pinType; bool vacuumAssisted = true; std::string description; };
struct AlignerPhysical { double chuckDiameter = 0.0, bodyDiameter = 0.0, height = 0.0, weight = 0.0; std::string weightUnit, mountingPattern, description; };
struct AlignerUnits { std::string length, angle, linearVelocity, angularVelocity, linearAcceleration, angularAcceleration, time; };
struct AlignerLoadType { std::string model; int loadTypeId = 0; std::string loadType; std::vector<int> waferSizes; std::string description; };
struct AlignerVariant { std::string model; int axisCount = 0; std::string loadType; std::vector<int> waferSizes; std::string description; };
struct AlignerMetadata { std::vector<AlignerVariant> variants; std::vector<std::string> notes; };

class Aligner : public Component {
public:
    Aligner() = default;
    Aligner(const std::string& filePath);
    bool loadFromFile(const std::string& filePath) override;
    void dump() const override;
    std::string model, manufacturer, description;
    int axisCount = 0; std::string loadType;
    AlignerUnits units; AlignerAlignment alignment; AlignerPinAssembly pinAssembly;
    std::string visual; AlignerPhysical physical;
    std::vector<std::string> compatibleEndEffectors;
    std::vector<ComponentAxis> axes; ComponentHoming homing;
    std::vector<IoPinRequired> ioPinsRequired; AlignerMetadata metadata;

    bool moveTo(double angle, double speed);
    bool home(); bool stopMotion(); bool emergencyStop();
    bool alignWafer(double angle, int settleMs);
};
