cmake_minimum_required(VERSION 3.29)
project(Noclip VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
if(NOT DEFINED ENV{GEODE_SDK})
  message(FATAL_ERROR "GEODE_SDK missing")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode-sdk)
# With current 2.2081 bindings the generated unity sources only forward-declare
# two state types. Force their complete generated definitions into that
# target's compilation units (GeodeBindings only, not the mod or SDK).
if(TARGET GeodeBindings AND WIN32)
    target_compile_options(GeodeBindings PRIVATE
        -include Geode/binding/SequenceTriggerState.hpp
        -include Geode/binding/EffectManagerState.hpp
    )
endif()

add_library(${PROJECT_NAME} SHARED src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE src)
setup_geode_mod(${PROJECT_NAME})
