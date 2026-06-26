#pragma once

#include <string>
#include <vector>

// =============================================================================
// Database — SQLite local pour ThorV4
// =============================================================================
//
// Crée/ouvre une base SQLite nommée d'après la WorkCell.
// Gère la persistance des teach points, l'historique d'exécution, etc.
//
// Usage :
//   Database db;
//   db.open("CELL-ATM100-001");
//   db.createTeachPoint("home", "robot", "joint");
//   bool taught = db.isTeachPointTaught("home");
// =============================================================================

/// @brief Teach point stocké en DB et en cache mémoire.
struct CachedTeachPoint
{
    std::string name;             // ex: "home", "alignerApproach"
    std::string componentId;      // ex: "robot", "aligner"
    std::string componentType;    // ex: "robot", "pre_aligner"
    std::string coordinateFrame;  // "joint" ou "cartesian"
    std::vector<double> jointPositions; // positions articulaires [j1, j2, ...]
    double cartesianX = 0.0;
    double cartesianY = 0.0;
    double cartesianZ = 0.0;
    bool isTaught = false;        // true si une position a été enseignée
};

/// @brief Gestionnaire de base de données SQLite.
class Database
{
public:
    Database();
    ~Database();

    // Non-copiable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// @brief Ouvre (ou crée) la base de données.
    /// @param dbName Nom du fichier (ex: "CELL-ATM100-001").
    /// @return true si ouvert/créé avec succès.
    bool open(const std::string& dbName);

    /// @brief Ferme la base de données.
    void close();

    /// @brief Vérifie si la base est ouverte.
    bool isOpen() const;

    /// @brief Dernier message d'erreur.
    std::string lastError() const;

    // --- Teach Points ---

    /// @brief Crée un teach point s'il n'existe pas déjà.
    /// @param name Nom du point (dbKey du WorkCell.json).
    /// @param componentId ID du composant (ex: "robot").
    /// @param componentType Type du composant (ex: "robot").
    /// @param coordinateFrame "joint" ou "cartesian".
    /// @return true si créé (ou déjà existant).
    bool createTeachPointIfNeeded(const std::string& name,
                                   const std::string& componentId,
                                   const std::string& componentType,
                                   const std::string& coordinateFrame);

    /// @brief Charge tous les teach points en mémoire.
    /// @param outPoints Vecteur rempli avec les points trouvés.
    /// @return true si chargé avec succès.
    bool loadAllTeachPoints(std::vector<CachedTeachPoint>& outPoints);

    /// @brief Sauvegarde un teach point (met à jour ou insère).
    /// @param point Point à sauvegarder.
    /// @return true si sauvegardé.
    bool saveTeachPoint(const CachedTeachPoint& point);

    /// @brief Vérifie si un teach point a été enseigné.
    bool isTeachPointTaught(const std::string& name);

private:
    void* mDb;            // sqlite3* (opaque, pas d'include sqlite3.h dans le header)
    std::string mDbPath;
    std::string mLastError;

    bool createSchema();
    bool executeSql(const std::string& sql);
};
