cmake_minimum_required(VERSION 3.22)
project(Aculyre VERSION 0.7.5 LANGUAGES CXX)

# An experimental prototype: the DSP and its Performer, no plug-in yet.
option(BUILD_TESTING "Build the tests" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(AculyreDSP STATIC Source/DSP/AculyreEngine.cpp Source/DSP/AculyrePerformer.cpp)
target_include_directories(AculyreDSP PUBLIC Source/DSP)
if(NOT MSVC)
    target_compile_options(AculyreDSP PRIVATE -Wall -Wextra)
endif()

# The Rack toolchain is C++17 without exceptions: prove the DSP builds there.
add_library(AculyreDSPCxx17 STATIC Source/DSP/AculyreEngine.cpp Source/DSP/AculyrePerformer.cpp)
set_target_properties(AculyreDSPCxx17 PROPERTIES CXX_STANDARD 17)
if(NOT MSVC)
    target_compile_options(AculyreDSPCxx17 PRIVATE -fno-exceptions -Wall -Wextra)
endif()

add_executable(RenderAculyreDemo Tools/RenderDemos.cpp)
target_link_libraries(RenderAculyreDemo PRIVATE AculyreDSP)

add_executable(RenderAculyreNotes Tools/RenderNotes.cpp)
target_link_libraries(RenderAculyreNotes PRIVATE AculyreDSP)

if(BUILD_TESTING)
    enable_testing()
    add_executable(AculyreEngineTests Tests/AculyreEngineTests.cpp)
    target_link_libraries(AculyreEngineTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.Engine COMMAND AculyreEngineTests)
    set_tests_properties(Aculyre.Engine PROPERTIES LABELS unit)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        # The committed demo phrases are what the writer writes (G3a).
        add_test(NAME Aculyre.DemoMidiSources
                 COMMAND "${Python3_EXECUTABLE}" -B
                         "${CMAKE_CURRENT_SOURCE_DIR}/Tools/write_preset_midi.py" --check)
        set_tests_properties(Aculyre.DemoMidiSources PROPERTIES LABELS unit)
    endif()
endif()
