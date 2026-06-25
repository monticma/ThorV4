#include "Agent/Scripting/LuaEngine.h"

#include <iostream>
#include <fstream>
#include <sol/sol.hpp>

// =============================================================================
// Fonctions globales exposées aux scripts Lua
// =============================================================================

/// @brief Fonction thorLog(level, message) accessible depuis Lua.
///
/// Usage Lua :
///   thorLog("INFO", "Robot initialisé")
///   thorLog("ERROR", "Échec du homing")
///
/// @param level   Niveau de log ("DEBUG", "INFO", "WARN", "ERROR").
/// @param message Message à logger.
static void thorLogFunction(const std::string& level, const std::string& message)
{
    // Pour l'instant, on loggue sur stderr. Plus tard, on pourra
    // router vers le système de log de l'Agent.
    std::cerr << "[Thor][" << level << "] " << message << std::endl;
}

// =============================================================================
// LuaEngine
// =============================================================================

LuaEngine::LuaEngine()
{
    // mLua est construit automatiquement (VM vide)
}

LuaEngine::~LuaEngine()
{
    // sol::state se détruit proprement, pas d'action nécessaire
}

bool LuaEngine::initialize()
{
    try {
        // Ouvrir uniquement les bibliothèques sûres (pas de os, io, debug)
        mLua.open_libraries(
            sol::lib::base,      // print, error, assert, tonumber, tostring...
            sol::lib::math,      // math.sin, math.random...
            sol::lib::string,    // string.format, string.sub...
            sol::lib::table      // table.insert, table.sort...
        );

        // Enregistrer les fonctions globales supplémentaires
        registerGlobals();

        return true;
    } catch (const std::exception& e) {
        mLastError = std::string("LuaEngine::initialize: ") + e.what();
        return false;
    }
}

void LuaEngine::registerGlobals()
{
    // Fonction de log accessible depuis les scripts
    mLua.set_function("thorLog", thorLogFunction);
}

bool LuaEngine::loadScriptFile(const std::string& filePath)
{
    try {
        mLua.script_file(filePath);
        return true;
    } catch (const sol::error& e) {
        mLastError = std::string("Lua error in ") + filePath + ": " + e.what();
        return false;
    }
}

bool LuaEngine::runString(const std::string& code)
{
    try {
        mLua.script(code);
        return true;
    } catch (const sol::error& e) {
        mLastError = std::string("Lua error: ") + e.what();
        return false;
    }
}

sol::state& LuaEngine::state()
{
    return mLua;
}

std::string LuaEngine::lastError() const
{
    return mLastError;
}
