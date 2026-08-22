cmake_minimum_required(VERSION 3.13)

set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libs/borealis/library)

# build options
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Dependencies
option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

# toolchain
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# project info
project(parallel_launcher)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_ALTER "0")
set(VERSION_BUILD "0")
set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
set(PACKAGE_NAME "ca.parallel_launcher.nx")
set(PROJECT_AUTHOR "Parallel Launcher Team")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/app_icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

add_subdirectory(libs/borealis/library)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# Source files
set(MAIN_SRC
    src/main.cpp

    src/core/catalog.cpp
    src/core/checksum.cpp
    src/core/emulator.cpp
    src/core/installer.cpp
    src/core/patch.cpp
    src/core/paths.cpp
    src/core/rom.cpp
    src/core/rom_service.cpp
    src/core/settings.cpp
    src/core/text_util.cpp

    src/emu/input.cpp

    src/ui/theme.cpp
    src/ui/main_activity.cpp

    src/ui/tabs/library_tab.cpp
    src/ui/tabs/browse_tab.cpp
    src/ui/tabs/account_tab.cpp
    src/ui/tabs/settings_tab.cpp

    src/ui/views/widgets.cpp
    src/ui/views/hack_card.cpp
    src/ui/views/hack_list_panel.cpp
    src/ui/views/hack_detail_view.cpp
)

if (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC ${CMAKE_SOURCE_DIR}/libs/borealis/library/lib/platforms/switch/switch_wrapper.c)
endif ()

program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${APP_VERSION}")
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BOREALIS_LIBRARY}/include
    ${BOREALIS_LIBRARY}/include/borealis/extern
    ${BOREALIS_LIBRARY}/include/borealis/extern/libretro-common
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    borealis
    -lnx
    -lm
    -lz
)

if (PLATFORM_SWITCH)
    nx_generate_nacp(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.nacp"
        NAME "ParaLLEl Launcher NX"
        AUTHOR "Parallel Launcher Team"
        VERSION "${APP_VERSION}"
    )
    nx_create_nro(${PROJECT_NAME}
        NACP "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.nacp"
        ICON "${PROJECT_ICON}"
        ROMFS "${PROJECT_RESOURCES}"
    )
endif ()
