cmake_minimum_required(VERSION 3.16)
project(ThorV4 VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dépendances système
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

# ------------------------------------------------------------------------------
# Vendored third-party libraries (nlohmann/json, Lua, CivetWeb, etc.)
# ------------------------------------------------------------------------------
add_subdirectory(Vendor)

# ------------------------------------------------------------------------------
# ThorLib — static library
# ------------------------------------------------------------------------------
add_library(ThorLib STATIC
    Src/Core/EventBus.cpp
    Src/Agent/Agent.cpp
    Src/Agent/Database.cpp
    Src/Agent/Workcell.cpp
    Src/Agent/Scripting/LuaEngine.cpp
    Src/Agent/Components/Component.cpp
    Src/Agent/Components/Controller.cpp
    Src/Agent/Components/Aligner.cpp
    Src/Agent/Components/Robot.cpp
    Src/Agent/Components/Track.cpp
    Src/Agent/Components/Cassette.cpp
    Src/Agent/Components/CycleCounter.cpp
    Src/Agent/Components/EndEffector.cpp
    Src/Agent/Components/Flipper.cpp
    Src/Agent/Components/ProcessZone.cpp
    Src/Agent/Components/SignalTower.cpp
    Src/Agent/Drivers/GalilProtocol.cpp
    Src/Agent/Drivers/GalilDriver.cpp
    Src/Agent/Kinematics/DHKinematics.cpp
)

target_include_directories(ThorLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

target_link_libraries(ThorLib PUBLIC
    thor_nlohmann   # nlohmann/json.hpp (header-only)
    thor_sol2       # sol2 + Lua 5.4.7 (header-only + static lib)
    SQLite::SQLite3 # SQLite3 (system)
)

# ------------------------------------------------------------------------------
# ThorAgent — executable
# ------------------------------------------------------------------------------
add_executable(ThorAgent
    Src/Agent/ThorAgent.cpp
)

target_link_libraries(ThorAgent PRIVATE
    ThorLib
)
