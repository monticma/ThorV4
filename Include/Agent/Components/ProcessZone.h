#pragma once

#include <string>
#include <vector>
#include <map>

#include "Component.h"

class Controller;
namespace sol
{
    class state;
}

// -----------------------------------------------------------------------------
// ProcessZone : chambre de process (ex: GenericChamber, CVD, Etch)
// -----------------------------------------------------------------------------

/// @brief Signal de handshake (direction + description).
struct ProcessZoneSignal {
    std::string direction;
    std::string description;
};

/// @brief Protocole de handshake de la zone de process.
struct ProcessZoneProtocol {
    std::string                         type;
    std::string                         description;
    std::map<std::string, ProcessZoneSignal> signals;
};

/// @brief Valeurs par défaut de la zone de process.
struct ProcessZoneDefaults {
    int         timeout     = 30000;
    std::string description;
};

/// @brief Propriétés physiques de la chambre.
struct ProcessZonePhysical {
    std::string              chamberType;
    std::vector<int>         waferSizes;
    std::string              description;
};

/// @brief Variante de modèle de chambre.
struct ProcessZoneVariant {
    std::string model;
    std::string chamberType;
    std::string description;
};

/// @brief Métadonnées de la zone de process.
struct ProcessZoneMetadata {
    std::vector<ProcessZoneVariant> variants;
    std::vector<std::string>        notes;
};

/// @brief Chambre de process (ex: GenericChamber, CVD, Etch).
class ProcessZone : public Component {
public:
    ProcessZone() = default;
    ProcessZone(const std::string& filePath);

    bool loadFromFile(const std::string& filePath) override;
    void dump() const override;

    std::string model;
    std::string manufacturer;
    std::string description;

    ProcessZoneProtocol        protocol;
    ProcessZoneDefaults        defaults;
    ProcessZonePhysical        physical;
    std::vector<IoPinRequired> ioPinsRequired;
    ProcessZoneMetadata        metadata;

    // Primitives (handshake I/O 5-wire)
    bool isReady();
    bool startProcess();
    bool waitProcessDone(int timeoutMs);
    bool hasError();
    bool resetError();

    void setController(Controller* controller) override;
    void registerInLua(sol::state& lua) override;

private:
    Controller* mController = nullptr;
    int getIoPin(const std::string& idPattern) const;
};
