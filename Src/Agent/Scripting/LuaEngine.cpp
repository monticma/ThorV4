#include "Agent/Scripting/LuaEngine.h"
#include "Core/EventBus.h"

#include <iostream>
#include <fstream>
#include <sol/sol.hpp>

// =============================================================================
// Fonctions globales exposées aux scripts Lua
// =============================================================================

/// @brief Pointeur statique vers l'EventBus pour thorPublish.
///        Initialisé par LuaEngine::setEventBus().
static EventBus* gEventBusForLua = nullptr;

/// @brief Fonction thorLog(level, message) accessible depuis Lua.
static void thorLogFunction(const std::string& level, const std::string& message)
{
    std::cerr << "[Thor][" << level << "] " << message << std::endl;
}

/// @brief Fonction thorPublish(eventType, message) accessible depuis Lua.
///
/// Publie un événement dans l'EventBus global. Usage Lua :
///   thorPublish("MOTION_COMPLETE", "Robot finished moveTo")
static void thorPublishFunction(const std::string& eventType,
                                 const std::string& message)
{
    if (gEventBusForLua == nullptr)
        return;

    Event event;
    event.type = EventType::ScriptStarted;
    event.priority = EventPriority::Normal;
    // Copier le type d'événement dans scriptName (champ 64 chars)
    size_t nameLen = eventType.length();
    if (nameLen > 63) nameLen = 63;
    for (size_t i = 0; i < nameLen; ++i)
        event.data.script.scriptName[i] = eventType[i];
    event.data.script.scriptName[nameLen] = '\0';
    // Copier le message
    size_t msgLen = message.length();
    if (msgLen > 255) msgLen = 255;
    for (size_t i = 0; i < msgLen; ++i)
        event.data.script.errorMessage[i] = message[i];
    event.data.script.errorMessage[msgLen] = '\0';
    event.data.script.lineNumber = 0;

    gEventBusForLua->publish(event);
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

    // Fonction de publication d'événements (initialement sans bus)
    mLua.set_function("thorPublish", thorPublishFunction);
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

void LuaEngine::setEventBus(EventBus* bus)
{
    mEventBus = bus;
    gEventBusForLua = bus;
}
