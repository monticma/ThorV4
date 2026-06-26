thorLog("INFO","MyScript.lua loaded")
if robot then
    thorLog("INFO","Robot model: " .. robot.model)

    -- moveTo direct (moteurs déjà ON via l'init du contrôleur)
    local ok = robot:moveTo(100, 100, 100, 50)
    thorLog("INFO", "moveTo() = " .. tostring(ok))
    if not ok then
        thorLog("ERROR", "moveTo failed: " .. robot:lastError())
    end

    -- Attendre
    robot:waitMotionDone(3000)

    local x, y, z = robot:getPosition()
    print("Robot moved to position: " .. x .. ", " .. y .. ", " .. z)
else
    thorLog("WARN","Robot not available")
end

