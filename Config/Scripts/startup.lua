-- =============================================================================
-- ThorV4 — Script de démarrage (startup.lua)
-- =============================================================================
-- Exécuté automatiquement après l'initialisation des composants.
-- Tous les composants sont déjà enregistrés dans Lua :
--   robot, track, aligner, flipper, endEffector1, signalTower, cycleCounter
-- Fonctions globales disponibles :
--   thorLog(level, message)      -- log côté C++
--   thorPublish(event, message)  -- publie un événement dans l'EventBus
-- =============================================================================

thorLog("INFO", "ThorV4 startup script loaded")

-- Vérifier que les composants sont accessibles
if robot then
    thorLog("INFO", "Robot '" .. robot.model .. "' ready")
else
    thorLog("WARN", "Robot not available")
end

if track then
    thorLog("INFO", "Track '" .. track.model .. "' ready")
end

if aligner then
    thorLog("INFO", "Aligner '" .. aligner.model .. "' ready")
end

thorPublish("STARTUP_COMPLETE", "All components initialized")

thorLog("INFO", "Startup sequence complete")
