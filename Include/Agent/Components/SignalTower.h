#pragma once

#include <string>
#include <vector>
#include <map>

#include "Component.h"

// -----------------------------------------------------------------------------
// SignalTower : colonne de signalisation (ex: PATLITE LR6-3)
// -----------------------------------------------------------------------------

/// @brief Configuration des lampes du signal tower.
struct SignalTowerLights {
    int                      count  = 3;
    std::vector<std::string> colors;
    std::vector<std::string> modes;
    std::string              description;
};

/// @brief Configuration du buzzer.
struct SignalTowerBuzzer {
    bool                     available = true;
    std::vector<std::string> modes;
    std::string              description;
};

/// @brief Valeurs par défaut du signal tower.
struct SignalTowerDefaults {
    int         blinkRateHz       = 2;
    int         buzzerFrequencyHz = 2800;
    std::string initialPattern;
};

/// @brief Pattern lumineux/sonore nommé.
struct SignalTowerPattern {
    std::string green;
    std::string yellow;
    std::string red;
    std::string blue;
    std::string white;
    std::string buzzer;
};

/// @brief Propriétés physiques du signal tower.
struct SignalTowerPhysical {
    double      height          = 0.0;
    double      diameter        = 0.0;
    double      weight          = 0.0;
    std::string weightUnit;
    std::string voltage;
    std::string mountingType;
    std::string protectionRating;
    std::string description;
};

/// @brief Variante de modèle de signal tower.
struct SignalTowerVariant {
    std::string              model;
    int                      lightCount = 3;
    std::vector<std::string> colors;
    std::string              description;
};

/// @brief Métadonnées du signal tower.
struct SignalTowerMetadata {
    std::vector<SignalTowerVariant> variants;
    std::vector<std::string>        notes;
};

/// @brief Colonne de signalisation (ex: PATLITE LR6-3).
class SignalTower : public Component {
public:
    SignalTower() = default;
    SignalTower(const std::string& filePath);

    bool loadFromFile(const std::string& filePath) override;
    void dump() const override;

    std::string model;
    std::string manufacturer;
    std::string description;

    SignalTowerLights                         lights;
    SignalTowerBuzzer                         buzzer;
    SignalTowerDefaults                       defaults;
    std::map<std::string, SignalTowerPattern> patterns;
    std::map<std::string, std::string>        reactions;
    SignalTowerPhysical                       physical;
    std::vector<IoPinRequired>                ioPinsRequired;
    SignalTowerMetadata                       metadata;
};
