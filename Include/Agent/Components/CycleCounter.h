#pragma once

#include <string>
#include <vector>
#include <map>

#include "Component.h"

// -----------------------------------------------------------------------------
// CycleCounter : compteur de cycles / erreurs via EventBus
// -----------------------------------------------------------------------------

/// @brief Paramètres du cycle counter (réactions aux événements).
struct CycleCounterParams {
    std::map<std::string, std::string> reactions;
};

/// @brief Métadonnées du cycle counter.
struct CycleCounterMetadata {
    std::vector<std::string> notes;
};

/// @brief Compteur de cycles / erreurs via EventBus.
class CycleCounter : public Component {
public:
    CycleCounter() = default;
    CycleCounter(const std::string& filePath);

    bool loadFromFile(const std::string& filePath) override;
    void dump() const override;

    std::string description;

    CycleCounterParams        params;
    std::vector<IoPinRequired> ioPinsRequired;
    CycleCounterMetadata      metadata;
};
