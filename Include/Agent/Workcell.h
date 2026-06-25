#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <nlohmann/json.hpp>

#include "Components/Component.h"

class EventBus;
namespace sol
{
    class state;
}

// -----------------------------------------------------------------------------
// Workcell : cellule de travail, contient tous les composants et le workflow
// Lecture du .json dans Config/WorkCells/
// -----------------------------------------------------------------------------

// --- Units ---

/// @brief Unités globales de la cellule de travail.
struct WorkcellUnits {
    std::string length;
    std::string angle;
    std::string linearVelocity;
    std::string angularVelocity;
    std::string linearAcceleration;
    std::string angularAcceleration;
    std::string time;
    std::string pressure;
    std::string speed;
    std::string description;
};

// ---------------------------------------------------------------------------
// Structs instance WorkCell (spécifiques à cette cellule, pas au template)
// ---------------------------------------------------------------------------

/// @brief Un teachpoint nommé — les coordonnées réelles sont en base agent.
struct TeachPoint {
    std::string dbKey;
    std::string coordinateFrame;
    bool        required    = true;
    std::string description;
};

/// @brief Assignation d'une pin I/O dans la WorkCell (pin physique).
struct IoPinAssignment {
    std::string id;
    int         pin = -1;
    std::string description;
};

/// @brief Entrée du mapping contrôleur : associe un axe contrôleur à un axe composant.
struct AxisMappingEntry {
    std::string controllerAxis;
    std::string component;
    std::string axis;          // référence axes[].name dans le template du composant
    std::string role;
    double      countsPerUnit = 1.0;
    std::string description;
};

/// @brief Connexion réseau d'un contrôleur (instance WorkCell).
/// @brief Canal de connexion réseau (adresse + port).
struct WorkcellConnectionChannel {
    std::string address;
    int         port = 0;
};

/// @brief Configuration de connexion réseau d'un contrôleur dans la WorkCell.
struct WorkcellConnection {
    std::string                          protocol;
    std::map<std::string, WorkcellConnectionChannel> channels;
    int                                  timeout = 5000;
};

/// @brief Position physique d'un composant dans la cellule.
struct WorkcellComponentPosition {
    double      x     = 0.0;
    double      y     = 0.0;
    double      z     = 0.0;
    double      theta = 0.0;
    std::string description;
};

/// @brief Offset d'approche.
struct WorkcellApproachOffset {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// @brief Configuration d'approche instance.
struct WorkcellComponentApproach {
    WorkcellApproachOffset offset;
    int                    speed = 30;
    std::string            description;
};

/// @brief Stratégie de rétractation instance.
struct WorkcellComponentRetract {
    std::string strategy;
    double      clearance    = 15.0;
    std::string description;
};

/// @brief Gestion d'erreur instance.
struct WorkcellComponentErrorHandling {
    int         retryCount       = 0;
    int         retryDelay       = 0;
    std::string onFailure;
    bool        retractOnError   = false;
    bool        notifyTowerLight = false;
    std::string logLevel;
    std::string description;
};

/// @brief Vitesses instance (en % du max template).
struct WorkcellComponentSpeeds {
    int         approach = 30;
    int         place    = 15;
    int         pick     = 15;
    std::string description;
};

/// @brief Paramètres de scan laser instance.
struct WorkcellComponentScanning {
    bool        enabled         = true;
    std::string direction;
    int         startSlot       = 25;
    int         endSlot         = 1;
    double      scanSpeed       = 10.0;
    double      scanStartZ      = 0.0;
    double      scanEndZ        = 0.0;
    double      filterThreshold = 0.5;
    std::string description;
};

/// @brief Point 3D réutilisable (workspace, scanner offset, etc.).
struct WorkcellPoint3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// @brief Configuration scanner (endEffector).
struct WorkcellScannerConfig {
    bool                enabled = true;
    WorkcellPoint3D     offsetFromTCP;
    std::vector<IoPinAssignment> ioPins;
    double              beamWidth   = 1.0;
    std::string         description;
};

/// @brief Entrée dans le tableau components[] de la WorkCell.
/// Contient les champs de base, les overrides instance, les I/O pins,
/// les teach points, et un pointeur vers l'instance Component chargée.
struct WorkcellComponentEntry {
    // Champs de base
    std::string              id;
    std::string              type;
    std::string              config;
    std::string              controller;
    std::vector<std::string> controllerAxes;
    bool                     enabled     = true;
    std::string              description;
    std::string              attachedTo;
    std::string              mutuallyExclusiveWith;

    // Controller uniquement
    WorkcellConnection               connection;
    std::vector<AxisMappingEntry>    axisMapping;

    // Teach points (robot, track, flipper...)
    std::map<std::string, TeachPoint> teachPoints;

    // Overrides / params libres
    nlohmann::json overrides;
    nlohmann::json params;

    // I/O pins (tous composants)
    std::vector<IoPinAssignment> ioPins;

    // Position / approche / rétractation (cassette...)
    WorkcellComponentPosition    position;
    WorkcellComponentApproach    approach;
    WorkcellComponentRetract     retract;

    // Gestion d'erreur
    WorkcellComponentErrorHandling errorHandling;

    // Vitesses instance
    WorkcellComponentSpeeds speeds;

    // Scan (cassette)
    WorkcellComponentScanning scanning;

    // Scanner (endEffector)
    WorkcellScannerConfig scanner;

    // Pointeur vers l'instance du composant chargé
    std::unique_ptr<Component> instance;
};

/// @brief Interface opérateur (bouton physique de resume).
struct WorkcellOperatorInterface {
    int         resumeButtonPin        = 0;
    std::string resumeButtonController;
    std::string description;
};

/// @brief Vitesses globales du workflow (en % du max).
struct WorkcellSpeeds {
    int         transit       = 100;
    int         approach      = 30;
    int         insertCassette = 15;
    int         scan          = 10;
    std::string description;
};

/// @brief Stratégie de rétractation par défaut pour le workflow.
struct WorkcellRetract {
    std::string defaultStrategy;
    double      defaultClearance     = 15.0;
    bool        retractBeforeRotation = true;
    std::string description;
};

/// @brief Temporisations globales du workflow (en ms).
struct WorkcellTimings {
    int         vacuumSettle       = 200;
    int         placeDwell          = 100;
    int         pickDwell           = 100;
    int         cassetteSwapTimeout = 120000;
    std::string description;
};

/// @brief Configuration du workflow de la cellule.
struct WorkcellWorkflow {
    std::string              defaultSequence;
    std::string              processingOrder;
    bool                     scanBeforeEachCassette = true;
    bool                     alignBeforeProcess     = true;
    bool                     returnAfterProcess     = true;
    bool                     returnToSameSlot       = true;
    bool                     pauseOnCassetteExhausted = true;
    bool                     autoResumeOnNewCassette  = true;
    WorkcellOperatorInterface operatorInterface;
    WorkcellSpeeds            speeds;
    WorkcellRetract           retract;
    WorkcellTimings           timings;
};

// --- Workspace ---

/// @brief Limites physiques de la cellule (boîte englobante).
struct WorkcellBoundary {
    WorkcellPoint3D min;
    WorkcellPoint3D max;
    std::string     description;
};

/// @brief Zone d'exclusion (cylindre ou boîte) pour éviter les collisions.
struct WorkcellExclusionZone {
    std::string      name;
    std::string      type;        // "cylinder" ou "box"
    WorkcellPoint3D  center;      // pour cylinder
    double           radius = 0.0;
    double           height = 0.0;
    WorkcellPoint3D  min;         // pour box
    WorkcellPoint3D  max;         // pour box
    std::string      description;
};

/// @brief Espace de travail : limite cellule + zones d'exclusion.
struct WorkcellWorkspace {
    WorkcellBoundary                   cellBoundary;
    std::vector<WorkcellExclusionZone> exclusionZones;
};

// --- Metadata ---

/// @brief Métadonnées de la WorkCell (version, dates, notes).
struct WorkcellMetadata {
    std::string              configVersion;
    std::string              created;
    std::string              modified;
    std::vector<std::string> notes;
};

// =============================================================================

/// @brief Cellule de travail : contient tous les composants et le workflow.
class Workcell {
public:
    Workcell() = default;

    /// @brief Construit et charge une Workcell depuis un fichier JSON.
    /// @param filePath Chemin vers le fichier WorkCell .json.
    Workcell(const std::string& filePath);
    ~Workcell();

    /// @brief Charge la configuration depuis un fichier JSON.
    /// @param filePath Chemin vers le fichier .json.
    /// @return true si le chargement a réussi.
    bool loadFromFile(const std::string& filePath);

    /// @brief Retourne le dernier message d'erreur.
    std::string lastError() const { return mLastError; }

    /// @brief Retourne la liste des composants de la cellule.
    const std::vector<WorkcellComponentEntry>& getComponents() const { return mComponents; }

    /// @brief Recherche un composant par son identifiant.
    /// @param id Identifiant du composant.
    /// @return Pointeur vers le composant, ou nullptr si non trouvé.
    Component* findComponent(const std::string& id) const;

    /// @brief Affiche la liste des composants sur la sortie standard.
    void listComponents() const;

    /// @brief Affiche TOUS les champs de la Workcell et de tous ses composants
    /// sur la sortie standard, pour vérifier la conformité avec les fichiers JSON.
    void dump() const;

    // --- Nouveau : assemblage et cycle de vie (V4) ---

    /// @brief Enregistre tous les composants dans la VM Lua.
    /// @param lua VM Lua (sol::state) dans laquelle enregistrer les types.
    /// @return true si toutes les inscriptions ont réussi.
    bool loadComponents(sol::state& lua);

    /// @brief Câble les composants entre eux : injecte les Controller*
    /// dans les Robot/Track/Aligner, et connecte les contrôleurs en TCP.
    /// @param bus EventBus utilisé pour les listeners asynchrones.
    /// @return true si le câblage a réussi.
    bool wireComponents(EventBus* bus);

    /// @brief Démarre les listeners asynchrones (GalilDriver port 2324).
    void startListeners();

    /// @brief Arrête tous les composants et déconnecte les contrôleurs.
    void shutdown();

    // --- Champs racine ---
    std::string version;
    std::string fileType;
    std::string cellId;
    std::string cellType;
    std::string description;

    // --- Sections ---
    WorkcellUnits     units;
    WorkcellWorkflow  workflow;
    WorkcellWorkspace workspace;
    WorkcellMetadata  metadata;

private:
    std::vector<WorkcellComponentEntry> mComponents;
    std::string mLastError;
    EventBus* mEventBus = nullptr;   // stocké pour startListeners()
};
