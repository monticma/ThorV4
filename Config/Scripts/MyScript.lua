thorLog("INFO","MyScript.lua loaded")
if robot then
    thorLog("INFO","Robot model: " .. robot.model)

    -- Lister les teach points
    if tp then
        thorLog("INFO", "Teach points disponibles:")
        for name, point in pairs(tp) do
            local taught = point.isTaught and "TAUGHT" or "empty"
            thorLog("INFO", "  tp." .. name .. " [" .. taught .. "]")
        end
    end

    -- moveTo avec coordonnées directes
    local ok = robot:moveTo(100, 100, 100, 50)
    thorLog("INFO", "moveTo(100,100,100) = " .. tostring(ok))
    local x, y, z = robot:getPosition()
    print("Robot moved to position: " .. x .. ", " .. y .. ", " .. z)
else
    thorLog("WARN","Robot not available")
end

