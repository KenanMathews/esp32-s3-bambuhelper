cmake_minimum_required(VERSION 3.16)
project(BambuHelperSimulator C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

# Reuse LVGL 9 and SDL2 from the shared simulator base
set(SIM_BASE "${CMAKE_SOURCE_DIR}/../../../esp32-lvgl-simulator")

find_package(SDL2 REQUIRED)

# Lua 5.4 via pkg-config (installed with Homebrew or system package manager)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED lua5.4)

# LVGL
add_subdirectory(${SIM_BASE}/lvgl ${CMAKE_BINARY_DIR}/lvgl)
target_include_directories(lvgl PUBLIC ${SIM_BASE} ${SDL2_INCLUDE_DIRS})

# BambuHelper SDK implementation
add_library(bh_sdk STATIC
    sdk_impl/sdk_bambu.c
    sdk_impl/sdk_ui.c
    sdk_impl/sdk_sys.c
)
target_include_directories(bh_sdk PUBLIC sdk_impl ${LUA_INCLUDE_DIRS} ${SIM_BASE}/lvgl)
target_link_libraries(bh_sdk lvgl ${LUA_LIBRARIES})

# Simulator executable
add_executable(simulator main.c ${SIM_BASE}/mouse_cursor_icon.c)
target_compile_definitions(simulator PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_include_directories(simulator PRIVATE ${LUA_INCLUDE_DIRS} sdk_impl)
target_link_libraries(simulator lvgl lvgl::examples lvgl::demos bh_sdk ${LUA_LIBRARIES} ${SDL2_LIBRARIES} m pthread)
target_link_directories(simulator PRIVATE ${LUA_LIBRARY_DIRS})

add_custom_target(run COMMAND ${EXECUTABLE_OUTPUT_PATH}/simulator DEPENDS simulator)
