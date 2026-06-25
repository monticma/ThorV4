#pragma once

#include <string>
#include <vector>
#include <iostream>

// Forward declarations
class Controller;
namespace sol { class state; }

// -----------------------------------------------------------------------------
// Structs communs à tous les composants (axes, homing, I/O pins)
// -----------------------------------------------------------------------------

/// @brief Plage de déplacement d'un axe.
struct AxisTravel {
    double min        = 0.0;
    double max        = 0.0;
    bool   continuous = false;
};

/// @brief Axe standardisé pour tout composant mobile (Robot, Aligner, Track, Flipper...).
struct ComponentAxis {
    std::string name;
    std::string type;          // "revolute" ou "prismatic"
    std::string role;          // "rotation", "extension", "vertical", "translation", "centering"
    AxisTravel  travel;
    double      maxVelocity     = 0.0;
    double      maxAcceleration = 0.0;
    double      homePosition    = 0.0;
    double      upPosition      = 0.0;
    double      downPosition    = 0.0;
    std::string description;
};

/// @brief Configuration de homing pour un composant mobile.
struct ComponentHoming {
    std::string              method;
    std::string              direction;
    double                   velocity    = 0.0;
    std::vector<std::string> sequence;
    std::string              description;
};

/// @brief Définition d'une pin I/O requise par un template composant.
struct IoPinRequired {
    std::string id;
    std::string direction;    // "input" ou "output"
    std::string signalType;   // "digital", "analog", "pwm", "quadrature"
    int         count        = 1;
    std::string voltage;
    int         debounceMs   = 0;
    std::string description;
};

// =============================================================================

/// @brief Classe de base pour tous les composants.
/// Définit l'interface commune : chargement JSON et gestion d'erreur.
class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    /// @brief Charge la configuration depuis un fichier JSON.
    /// @param filePath Chemin vers le fichier .json.
    /// @return true si le chargement a réussi.
    virtual bool loadFromFile(const std::string& filePath) = 0;

    /// @brief Retourne le dernier message d'erreur.
    std::string lastError() const { return mLastError; }

    /// @brief Affiche tous les champs du composant sur la sortie standard.
    /// Chaque sous-classe doit surcharger cette méthode pour dumper ses champs spécifiques.
    virtual void dump() const;

    // =========================================================================
    // Injection de dépendances (nouveau)
    // =========================================================================

    /// @brief Reçoit le contrôleur auquel ce composant est rattaché.
    /// Surchargée par Robot, Track, Aligner. Ignorée par défaut.
    virtual void setController(Controller* controller) { (void)controller; }

    /// @brief Enregistre ce type de composant dans Lua.
    /// Chaque sous-classe DOIT surcharger pour exposer ses primitives.
    virtual void registerInLua(sol::state& lua) { (void)lua; }

    // Champs communs à tous les composants (lus par chaque loadFromFile)
    std::string version;
    std::string fileType;

protected:
    std::string mLastError;
};

// -----------------------------------------------------------------------------
// Fonctions helpers de dump — utilisées par les implémentations dump() des
// sous-classes de Component.
// -----------------------------------------------------------------------------

void dumpField(const std::string& key, const std::string& value, int indent = 2);
void dumpFieldInt(const std::string& key, int value, int indent = 2);
void dumpFieldDouble(const std::string& key, double value, int indent = 2);
void dumpFieldBool(const std::string& key, bool value, int indent = 2);
void dumpStringVector(const std::string& key, const std::vector<std::string>& vec, int indent = 2);
void dumpIntVector(const std::string& key, const std::vector<int>& vec, int indent = 2);
void dumpIoPin(const IoPinRequired& io, int indent = 6);
void dumpIoPins(const std::vector<IoPinRequired>& pins, const std::string& label = "ioPins", int indent = 4);
void dumpAxis(const ComponentAxis& ax, int indent = 6);
void dumpAxes(const std::vector<ComponentAxis>& axes, const std::string& label = "axes", int indent = 4);
void dumpHoming(const ComponentHoming& h, int indent = 4);
