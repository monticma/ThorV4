#pragma once
#include <string>
#include <vector>
#include "Component.h"

struct CassetteUnits
{
    std::string length;
    std::string time;
};
struct CassetteGeometry
{
    int slotCount = 25;
    double pitch = 0.0, offset = 0.0, stroke = 0.0, slotHeight = 0.0;
    int waferDiameter = 200;
    double waferThickness = 0.0;
    std::string description;
};
struct CassettePhysical
{
    double width = 0.0, depth = 0.0, height = 0.0, openingWidth = 0.0, openingHeight = 0.0, weight = 0.0;
    std::string weightUnit, description;
};
struct CassetteWaferThickness
{
    double min = 0.3, max = 1.2;
};
struct CassetteWaferSpecs
{
    int diameter = 200;
    CassetteWaferThickness thickness;
    std::vector<std::string> material;
    std::string description;
};
struct CassetteVariant
{
    std::string model, standard, description;
    int waferDiameter = 200;
    double pitch = 0.0;
    int slotCount = 25;
};
struct CassetteMetadata
{
    std::vector<CassetteVariant> variants;
    std::vector<std::string> notes;
};

class Cassette : public Component
{
public:
    Cassette() = default;
    Cassette(const std::string &filePath);
    bool loadFromFile(const std::string &filePath) override;
    void dump() const override;
    std::string model, standard, description;
    CassetteUnits units;
    CassetteGeometry geometry;
    CassettePhysical physical;
    CassetteWaferSpecs waferSpecs;
    std::vector<IoPinRequired> ioPinsRequired;
    CassetteMetadata metadata;
};
