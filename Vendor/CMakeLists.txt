# ==============================================================================
# Vendor / json-schema-validator — built as a static lib `ThorVendorSchema`
# ==============================================================================
#
# Why this exists:
#   ThorV3 disables exceptions on its hot paths and forbids PImpl elsewhere,
#   but boot-time JSON Schema validation is delegated to
#   pboettch/json-schema-validator (MIT) which is exception-based. We compile
#   it as a separate static library with exceptions enabled and relaxed
#   warnings; only `Src/Core/SchemaValidator.cpp` links against it and
#   catches the exceptions at the boundary.
#
# Pinned upstream:
#   - nlohmann/json            v3.11.3   (single-header at Vendor/nlohmann/json.hpp)
#   - pboettch/json-schema-validator 2.3.0 (sources at Vendor/json-schema-validator/src)
# ==============================================================================

set(JSV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/json-schema-validator/src)

set(JSV_SOURCES
    ${JSV_DIR}/json-patch.cpp
    ${JSV_DIR}/json-schema-draft7.json.cpp
    ${JSV_DIR}/json-uri.cpp
    ${JSV_DIR}/json-validator.cpp
    ${JSV_DIR}/smtp-address-validator.cpp
    ${JSV_DIR}/string-format-check.cpp
)

add_library(ThorVendorSchema STATIC ${JSV_SOURCES})

# Vendor code: relaxed warnings, exceptions ON.
target_compile_options(ThorVendorSchema PRIVATE
    -w                  # silence all warnings from third-party code
    -fexceptions
)

# Public include paths so consumers can `#include <nlohmann/json.hpp>` and
# `#include <nlohmann/json-schema.hpp>`.
target_include_directories(ThorVendorSchema
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}            # for <nlohmann/json.hpp>
        ${CMAKE_CURRENT_SOURCE_DIR}/json-schema-validator/src
                                               # for <nlohmann/json-schema.hpp>
)

set_target_properties(ThorVendorSchema PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ==============================================================================
# Header-only vendored libraries (interface targets)
# ==============================================================================

# nlohmann/json — single header included as <nlohmann/json.hpp>.
add_library(thor_nlohmann INTERFACE)
target_include_directories(thor_nlohmann INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# sol2 — Lua/C++ binding, header-only. SOL_ALL_SAFETIES_ON hardens every call.
add_library(thor_sol2 INTERFACE)
target_include_directories(thor_sol2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sol2)
target_compile_definitions(thor_sol2 INTERFACE SOL_ALL_SAFETIES_ON=1)
target_link_libraries(thor_sol2 INTERFACE thor_lua)

# ==============================================================================
# Lua 5.4.7 — vendored C core (no dlopen/readline: sandbox hardening)
# ==============================================================================
#
# Third-party C: built WITHOUT sanitizers (a non-instrumented static lib links
# cleanly into asan/tsan executables; instrumenting the C core with gcc-ubsan
# yields runtime symbols incompatible with the g++ link step). The standalone
# interpreter (lua.c) and compiler (luac.c) are excluded.

file(GLOB LUA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4.7/src/*.c)
list(FILTER LUA_SOURCES EXCLUDE REGEX "/(lua|luac)\\.c$")

add_library(thor_lua STATIC ${LUA_SOURCES})
target_compile_definitions(thor_lua PRIVATE LUA_USE_POSIX)
target_compile_options(thor_lua PRIVATE -w)
target_include_directories(thor_lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4.7/src)
set_target_properties(thor_lua PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)

# ==============================================================================
# CivetWeb 1.16 — embedded HTTPS/WSS server (MIT)
# ==============================================================================
#
# NO_SSL_DL links libssl directly (no dlopen); OPENSSL_API_3_0 targets OpenSSL
# 3.x; USE_WEBSOCKET enables the WS support used by WebSocketHub. Built without
# sanitizers (same rationale as Lua).

add_library(thor_civetweb STATIC ${CMAKE_CURRENT_SOURCE_DIR}/civetweb/src/civetweb.c)
target_compile_definitions(thor_civetweb PRIVATE
    NDEBUG NO_SSL_DL OPENSSL_API_3_0 USE_WEBSOCKET)
target_compile_options(thor_civetweb PRIVATE -w)
target_include_directories(thor_civetweb
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/civetweb/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/civetweb/src)
target_link_libraries(thor_civetweb PUBLIC OpenSSL::SSL OpenSSL::Crypto)
set_target_properties(thor_civetweb PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)

# ==============================================================================
# libbcrypt — bcrypt password hashing (public domain), built without sanitizers
# ==============================================================================

set(BCRYPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libbcrypt)
add_library(thor_bcrypt STATIC
    ${BCRYPT_DIR}/crypt_blowfish/crypt_blowfish.c
    ${BCRYPT_DIR}/crypt_blowfish/crypt_gensalt.c
    ${BCRYPT_DIR}/crypt_blowfish/wrapper.c
    ${BCRYPT_DIR}/bcrypt.c)
target_compile_definitions(thor_bcrypt PRIVATE NDEBUG)
target_compile_options(thor_bcrypt PRIVATE -w)
target_include_directories(thor_bcrypt
    PUBLIC  ${BCRYPT_DIR}
    PRIVATE ${BCRYPT_DIR}/crypt_blowfish)
set_target_properties(thor_bcrypt PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
