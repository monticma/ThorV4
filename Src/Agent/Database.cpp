#include "Agent/Database.h"

#include <sqlite3.h>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

// =============================================================================
// Database
// =============================================================================

Database::Database()
    : mDb(nullptr)
{
}

Database::~Database()
{
    close();
}

bool Database::open(const std::string& dbName)
{
    if (mDb != nullptr) {
        mLastError = "Database already open";
        return false;
    }

    // Créer le dossier Database/ s'il n'existe pas
    struct stat st;
    if (stat("Database", &st) != 0) {
        mkdir("Database", 0755);
    }

    mDbPath = "Database/" + dbName + ".db";

    int rc = sqlite3_open(mDbPath.c_str(), reinterpret_cast<sqlite3**>(&mDb));
    if (rc != SQLITE_OK) {
        mLastError = "sqlite3_open failed: "
                     + std::string(sqlite3_errmsg(static_cast<sqlite3*>(mDb)));
        sqlite3_close(static_cast<sqlite3*>(mDb));
        mDb = nullptr;
        return false;
    }

    // Activer WAL mode pour les performances
    executeSql("PRAGMA journal_mode=WAL");
    executeSql("PRAGMA foreign_keys=ON");

    // Créer le schéma si nécessaire
    if (!createSchema()) {
        close();
        return false;
    }

    return true;
}

void Database::close()
{
    if (mDb != nullptr) {
        sqlite3_close(static_cast<sqlite3*>(mDb));
        mDb = nullptr;
    }
}

bool Database::isOpen() const
{
    return mDb != nullptr;
}

std::string Database::lastError() const
{
    return mLastError;
}

// =============================================================================
// Schema
// =============================================================================

bool Database::createSchema()
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS teaching_points ("
        "  name TEXT PRIMARY KEY,"
        "  component_id TEXT NOT NULL DEFAULT '',"
        "  component_type TEXT NOT NULL DEFAULT '',"
        "  coordinate_frame TEXT NOT NULL DEFAULT 'joint',"
        "  joint_positions TEXT NOT NULL DEFAULT '[]',"
        "  cartesian_x REAL DEFAULT 0.0,"
        "  cartesian_y REAL DEFAULT 0.0,"
        "  cartesian_z REAL DEFAULT 0.0,"
        "  is_taught INTEGER DEFAULT 0,"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");";

    return executeSql(sql);
}

bool Database::executeSql(const std::string& sql)
{
    if (mDb == nullptr) {
        mLastError = "Database not open";
        return false;
    }

    char* errMsg = nullptr;
    int rc = sqlite3_exec(static_cast<sqlite3*>(mDb), sql.c_str(),
                          nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        mLastError = "SQL error: " + std::string(errMsg ? errMsg : "unknown");
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// =============================================================================
// Teach Points
// =============================================================================

bool Database::createTeachPointIfNeeded(const std::string& name,
                                         const std::string& componentId,
                                         const std::string& componentType,
                                         const std::string& coordinateFrame)
{
    if (mDb == nullptr) return false;

    // Vérifier si le point existe déjà
    const char* checkSql =
        "SELECT COUNT(*) FROM teaching_points WHERE name = ?;";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(mDb),
                                checkSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        mLastError = "prepare check: " +
                     std::string(sqlite3_errmsg(static_cast<sqlite3*>(mDb)));
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count > 0) {
        return true; // déjà existant
    }

    // Insérer le nouveau point
    const char* insertSql =
        "INSERT INTO teaching_points "
        "(name, component_id, component_type, coordinate_frame) "
        "VALUES (?, ?, ?, ?);";

    rc = sqlite3_prepare_v2(static_cast<sqlite3*>(mDb),
                            insertSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        mLastError = "prepare insert: " +
                     std::string(sqlite3_errmsg(static_cast<sqlite3*>(mDb)));
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, componentId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, componentType.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, coordinateFrame.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        mLastError = "insert teach point: " +
                     std::string(sqlite3_errmsg(static_cast<sqlite3*>(mDb)));
        return false;
    }

    return true;
}

bool Database::loadAllTeachPoints(std::vector<CachedTeachPoint>& outPoints)
{
    if (mDb == nullptr) return false;

    outPoints.clear();

    const char* sql = "SELECT name, component_id, component_type, "
                      "coordinate_frame, joint_positions, "
                      "cartesian_x, cartesian_y, cartesian_z, is_taught "
                      "FROM teaching_points ORDER BY name;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(mDb),
                                sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        mLastError = "loadAllTeachPoints prepare: " +
                     std::string(sqlite3_errmsg(static_cast<sqlite3*>(mDb)));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CachedTeachPoint tp;
        tp.name = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 0));
        tp.componentId = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 1));
        tp.componentType = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 2));
        tp.coordinateFrame = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 3));

        // Parser joint_positions (format JSON: "[1.0, 2.0, 3.0]")
        const char* jointsText = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 4));
        tp.jointPositions.clear();
        if (jointsText != nullptr && std::strlen(jointsText) > 2) {
            // Parser simple : valeurs séparées par virgules entre [ ]
            const char* p = jointsText + 1; // sauter '['
            while (*p != '\0' && *p != ']') {
                char* end = nullptr;
                double val = std::strtod(p, &end);
                if (end > p) {
                    tp.jointPositions.push_back(val);
                    p = end;
                }
                while (*p == ',' || *p == ' ') ++p;
            }
        }

        tp.cartesianX = sqlite3_column_double(stmt, 5);
        tp.cartesianY = sqlite3_column_double(stmt, 6);
        tp.cartesianZ = sqlite3_column_double(stmt, 7);
        tp.isTaught = (sqlite3_column_int(stmt, 8) != 0);

        outPoints.push_back(tp);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool Database::saveTeachPoint(const CachedTeachPoint& point)
{
    if (mDb == nullptr) return false;

    // Sérialiser jointPositions en JSON simple
    std::string jointsJson = "[";
    for (size_t i = 0; i < point.jointPositions.size(); ++i) {
        if (i > 0) jointsJson += ",";
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6f", point.jointPositions[i]);
        jointsJson += buf;
    }
    jointsJson += "]";

    const char* sql =
        "INSERT OR REPLACE INTO teaching_points "
        "(name, component_id, component_type, coordinate_frame, "
        " joint_positions, cartesian_x, cartesian_y, cartesian_z, "
        " is_taught, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'));";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(mDb),
                                sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        mLastError = "saveTeachPoint prepare: " +
                     std::string(sqlite3_errmsg(static_cast<sqlite3*>(mDb)));
        return false;
    }

    sqlite3_bind_text(stmt, 1, point.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, point.componentId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, point.componentType.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, point.coordinateFrame.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, jointsJson.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 6, point.cartesianX);
    sqlite3_bind_double(stmt, 7, point.cartesianY);
    sqlite3_bind_double(stmt, 8, point.cartesianZ);
    sqlite3_bind_int(stmt, 9, point.isTaught ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        mLastError = "saveTeachPoint: " +
                     std::string(sqlite3_errmsg(static_cast<sqlite3*>(mDb)));
        return false;
    }

    return true;
}

bool Database::isTeachPointTaught(const std::string& name)
{
    if (mDb == nullptr) return false;

    const char* sql = "SELECT is_taught FROM teaching_points WHERE name = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(mDb),
                                sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    bool taught = false;
    if (rc == SQLITE_ROW) {
        taught = (sqlite3_column_int(stmt, 0) != 0);
    }
    sqlite3_finalize(stmt);
    return taught;
}
