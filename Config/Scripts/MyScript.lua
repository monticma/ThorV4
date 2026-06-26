thorLog("INFO","MyScript.lua loaded")
if robot then
    thorLog("INFO","Robot model: " .. robot.model)

    robot:home()
    robot:moveTo(100, 100, 100, 50)
    local x, y, z = robot:getPosition()
    print("Robot moved to position: " .. x .. ", " .. y .. ", " .. z)
else
    thorLog("WARN","Robot not available")
end

