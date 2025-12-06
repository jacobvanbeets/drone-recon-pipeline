cmake_minimum_required(VERSION 3.20)
project(DroneRecon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static runtime linking for portability (no external DLLs needed)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /utf-8)
endif()

# Project structure
set(VENDOR_DIR "${CMAKE_SOURCE_DIR}/vendor")
set(CONFIG_DIR "${CMAKE_SOURCE_DIR}/config")

# Source files
set(SOURCES
    src/main.cpp
    src/gui.cpp
    src/pipeline.cpp
)

set(HEADERS
    include/gui.h
    include/pipeline.h
)

# Executable (WIN32 makes it a GUI app without console)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link Windows libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE comctl32 comdlg32 shell32 ole32)
endif()

# Copy config files and vendor directory to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CONFIG_DIR}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${VENDOR_DIR}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/vendor
)

# Installation target (optional)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY ${CONFIG_DIR}/ DESTINATION bin/config)
