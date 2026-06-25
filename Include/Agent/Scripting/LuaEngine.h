#pragma once

#include <string>
#include <sol/sol.hpp>

class EventBus;

// =============================================================================
// LuaEngine — Moteur Lua embarqué pour ThorV4
// =============================================================================
//
// Encapsule un sol::state (VM Lua 5.4.7 via sol2).
// Charge et exécute les scripts dans un environnement sandboxé.
//
// Sandbox :
//   - Bibliothèques ouvertes : base, math, string, table
//   - Bibliothèques fermées  : os, io, debug, package (sécurité)
//   - SOL_ALL_SAFETIES_ON    : vérifications strictes des types
//
// Usage :
//   LuaEngine engine;
//   engine.initialize();
//   engine.loadScriptFile("Config/Scripts/startup.lua");
//   // ou
//   engine.runString("print('hello')");
//
//   // Pour exposer des types C++ à Lua :
//   Robot::registerInLua(engine.state());
// =============================================================================

/// @brief Moteur d'exécution Lua sandboxé.
class LuaEngine
{
public:
    LuaEngine();
    ~LuaEngine();

    // Non-copiable
    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    /// @brief Initialise la VM Lua et ouvre les bibliothèques sandboxées.
    /// @return true si l'initialisation a réussi.
    bool initialize();

    /// @brief Exécute un script Lua depuis un fichier.
    /// @param filePath Chemin vers le fichier .lua.
    /// @return true si l'exécution a réussi.
    bool loadScriptFile(const std::string& filePath);

    /// @brief Exécute une chaîne de code Lua.
    /// @param code Code Lua à exécuter.
    /// @return true si l'exécution a réussi.
    bool runString(const std::string& code);

    /// @brief Accès à la VM Lua sous-jacente.
    ///
    /// Utilisé par les composants pour s'enregistrer via registerInLua().
    /// @return Référence vers le sol::state.
    sol::state& state();

    /// @brief Dernier message d'erreur.
    /// @return Chaîne vide si aucune erreur.
    std::string lastError() const;

    /// @brief Injecte l'EventBus pour la fonction thor.publish().
    /// @param bus Pointeur vers l'EventBus (non-possédant).
    void setEventBus(EventBus* bus);

private:
    sol::state  mLua;
    std::string mLastError;
    EventBus*   mEventBus = nullptr;

    /// @brief Enregistre les fonctions globales accessibles aux scripts Lua.
    void registerGlobals();
};
