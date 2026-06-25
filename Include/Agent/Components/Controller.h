#pragma once
#include <string>
#include <vector>
#include "Component.h"
#include "Agent/Drivers/GalilDriver.h"
namespace sol { class state; }

struct EncoderResolution { int min = 0, max = 0; std::string unit; };
struct ControllerCapabilities {
    int maxAxes = 0; std::vector<std::string> axisLabels;
    bool coordinatedMotion = false, linearInterpolation = false, circularInterpolation = false;
    bool electronicGearing = false, encoderFeedback = false, pidControl = false;
    EncoderResolution encoderResolution;
    std::vector<std::string> motionProfiles;
    int digitalInputs = 0, digitalOutputs = 0, analogInputs = 0, analogOutputs = 0, programMemory = 0;
    std::string firmwareLanguage;
};
struct TcpConnection { int defaultPort = 0, commandPort = 0, messagePort = 0; std::string description; };
struct SerialConnection { int defaultBaud = 0, dataBits = 0, stopBits = 0; std::string parity; };
struct ControllerConnection {
    std::vector<std::string> supportedProtocols; TcpConnection tcp; SerialConnection serial;
};
struct ControllerDriver { std::string identifier, library, driverConfig, description; };
struct ControllerUnits { std::string length, angle, linearVelocity, angularVelocity, linearAcceleration, angularAcceleration, time; };
struct ControllerLimits { double maxVelocity = 0.0, maxAcceleration = 0.0; std::string velocityUnit, accelerationUnit, description; };
struct ControllerMetadata { std::string datasheet; std::vector<std::string> notes; };
struct ControllerIoCapabilities {
    int digitalInputs = 0, digitalOutputs = 0, analogInputs = 0, analogOutputs = 0;
    std::string digitalVoltage, analogVoltage; int maxCurrentPerOutput = 0; std::string currentUnit, description;
};

class Controller : public Component {
public:
    Controller() = default;
    Controller(const std::string& filePath);
    bool loadFromFile(const std::string& filePath) override;
    void dump() const override;
    std::string model, manufacturer, family, description;
    ControllerUnits units; ControllerCapabilities capabilities;
    ControllerConnection connection; ControllerDriver driver;
    ControllerLimits limits; ControllerIoCapabilities ioCapabilities; ControllerMetadata metadata;

    bool connect(const std::string& address, int commandPort, int messagePort);
    void disconnect(); bool isConnected() const;
    GalilDriver* getDriver();

    // IO primitives
    bool setDigitalOutput(int pin, bool value);
    bool waitDigitalInput(int pin, bool expected, int timeoutMs);
    bool setAnalogOutput(int channel, double value);
    bool readAnalogInput(int channel, double& valueOut);

    void registerInLua(sol::state& lua) override;
private:
    GalilDriver mDriver;
};
