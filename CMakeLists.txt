cmake_minimum_required(VERSION 3.22)
project(Aculyre VERSION 0.13.14 LANGUAGES CXX)

# An experimental prototype: the DSP and its Performer, no plug-in yet.
option(BUILD_TESTING "Build the tests" ON)
option(ACULYRE_BUILD_TOOLS "Build the demo renderers and the analysis tools" ON)
option(ACULYRE_ENABLE_SANITIZERS "Instrument the DSP, tests and tools with ASan and UBSan" OFF)
option(ACULYRE_BUILD_FUZZER "Build AculyreDspFuzz as a Clang libFuzzer target" OFF)
option(ACULYRE_REQUIRE_PYTHON_TESTS
    "Fail to configure unless Python 3 is found for Aculyre.DemoMidiSources (CI sets it)" OFF)
option(ACULYRE_PROFILE_CALLGRIND "Collect CPU-probe callback instructions with Valgrind" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# U10: every target below, the DSP, its tests and the tools, built with the
# portfolio's sanitizer flags. Timing means nothing there, so the perf tier
# is not registered in such a build.
if(ACULYRE_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "ACULYRE_ENABLE_SANITIZERS needs Clang or GCC")
    endif()
    add_compile_options(-fsanitize=address,undefined,float-cast-overflow
                        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined,float-cast-overflow)
endif()

add_library(AculyreDSP STATIC Source/DSP/AculyreEngine.cpp Source/DSP/AculyrePerformer.cpp)
target_include_directories(AculyreDSP PUBLIC Source/DSP)
if(NOT MSVC)
    target_compile_options(AculyreDSP PRIVATE -Wall -Wextra -Werror)
endif()

# The Rack toolchain is C++17 without exceptions: prove the DSP builds there.
add_library(AculyreDSPCxx17 STATIC Source/DSP/AculyreEngine.cpp Source/DSP/AculyrePerformer.cpp)
set_target_properties(AculyreDSPCxx17 PROPERTIES CXX_STANDARD 17)
if(NOT MSVC)
    target_compile_options(AculyreDSPCxx17 PRIVATE -fno-exceptions -Wall -Wextra -Werror)
endif()

if(ACULYRE_BUILD_TOOLS)
    # One README demo per preset from Docs/audio/midi, and the More demos
    # from Docs/audio/midi/more (Tools/PresetDemoRenderer.h is the shared
    # portfolio copy; do not edit it here).
    find_package(Threads REQUIRED)
    add_executable(RenderAculyrePresetDemos Tools/RenderPresetDemos.cpp)
    target_link_libraries(RenderAculyrePresetDemos PRIVATE AculyreDSP Threads::Threads)

    add_executable(RenderAculyreDemo Tools/RenderDemos.cpp)
    target_link_libraries(RenderAculyreDemo PRIVATE AculyreDSP)

    add_executable(RenderAculyreNotes Tools/RenderNotes.cpp)
    target_link_libraries(RenderAculyreNotes PRIVATE AculyreDSP)

    add_executable(MeasureAculyreDesign Tools/MeasureDesign.cpp)
    target_link_libraries(MeasureAculyreDesign PRIVATE AculyreDSP)

    add_executable(AculyreKeyboardSurvey Tools/SurveyKeyboard.cpp)
    target_link_libraries(AculyreKeyboardSurvey PRIVATE AculyreDSP)

    add_executable(AculyreCpuProbe Tools/MeasureCpu.cpp)
    target_include_directories(AculyreCpuProbe PRIVATE Source)
    target_link_libraries(AculyreCpuProbe PRIVATE AculyreDSP)
    if(ACULYRE_PROFILE_CALLGRIND)
        find_path(VALGRIND_INCLUDE_DIR valgrind/callgrind.h REQUIRED)
        target_include_directories(AculyreCpuProbe PRIVATE "${VALGRIND_INCLUDE_DIR}")
        target_compile_definitions(AculyreCpuProbe PRIVATE ACULYRE_USE_CALLGRIND)
    endif()
endif()

if(ACULYRE_BUILD_FUZZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ACULYRE_BUILD_FUZZER requires Clang")
    endif()
    add_executable(AculyreDspFuzz Tests/DspFuzz.cpp Source/DSP/AculyreEngine.cpp)
    target_include_directories(AculyreDspFuzz PRIVATE Source/DSP)
    target_compile_definitions(AculyreDspFuzz PRIVATE _GLIBCXX_ASSERTIONS)
    target_compile_options(AculyreDspFuzz PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(AculyreDspFuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_executable(AculyreEngineTests Tests/AculyreEngineTests.cpp)
    target_link_libraries(AculyreEngineTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.Engine COMMAND AculyreEngineTests)
    set_tests_properties(Aculyre.Engine PROPERTIES LABELS unit)
    add_executable(AculyreModeBankTests Tests/ModeBankTests.cpp)
    target_include_directories(AculyreModeBankTests PRIVATE Source/DSP)
    add_test(NAME Aculyre.ModeBank COMMAND AculyreModeBankTests)
    set_tests_properties(Aculyre.ModeBank PROPERTIES LABELS unit)
    add_executable(AculyreDenormalTests Tests/DenormalTests.cpp)
    target_link_libraries(AculyreDenormalTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.Denormals COMMAND AculyreDenormalTests)
    set_tests_properties(Aculyre.Denormals PROPERTIES LABELS unit)
    add_executable(AculyreContactPhysicsTests Tests/ContactPhysicsTests.cpp)
    target_link_libraries(AculyreContactPhysicsTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.ContactPhysics COMMAND AculyreContactPhysicsTests)
    set_tests_properties(Aculyre.ContactPhysics PROPERTIES LABELS unit)
    add_executable(AculyreRangeSafetyTests Tests/RangeSafetyTests.cpp)
    target_link_libraries(AculyreRangeSafetyTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.RangeSafety COMMAND AculyreRangeSafetyTests)
    set_tests_properties(Aculyre.RangeSafety PROPERTIES LABELS unit)
    add_executable(AculyreRealismPhysicsTests Tests/RealismPhysicsTests.cpp)
    target_link_libraries(AculyreRealismPhysicsTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.RealismPhysics COMMAND AculyreRealismPhysicsTests)
    set_tests_properties(Aculyre.RealismPhysics PROPERTIES LABELS unit)
    add_executable(AculyreOwnershipTests Tests/OwnershipTests.cpp)
    target_link_libraries(AculyreOwnershipTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.Ownership COMMAND AculyreOwnershipTests)
    set_tests_properties(Aculyre.Ownership PROPERTIES LABELS unit)
    add_executable(AculyreInstrumentSwitchTests Tests/InstrumentSwitchTests.cpp)
    target_link_libraries(AculyreInstrumentSwitchTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.InstrumentSwitch COMMAND AculyreInstrumentSwitchTests)
    set_tests_properties(Aculyre.InstrumentSwitch PROPERTIES LABELS unit)
    add_executable(AculyreCapturePhysicsTests Tests/CapturePhysicsTests.cpp)
    target_link_libraries(AculyreCapturePhysicsTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.CapturePhysics COMMAND AculyreCapturePhysicsTests)
    set_tests_properties(Aculyre.CapturePhysics PROPERTIES LABELS unit)
    add_executable(AculyrePiezoCircuitTests Tests/PiezoCircuitTests.cpp)
    target_include_directories(AculyrePiezoCircuitTests PRIVATE Source)
    target_compile_definitions(AculyrePiezoCircuitTests PRIVATE
        ACULYRE_PIEZO_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/piezo-circuit")
    add_test(NAME Aculyre.PiezoCircuit COMMAND AculyrePiezoCircuitTests)
    set_tests_properties(Aculyre.PiezoCircuit PROPERTIES LABELS unit)
    add_executable(AculyrePiezoRoutingTests Tests/PiezoRoutingTests.cpp)
    target_link_libraries(AculyrePiezoRoutingTests PRIVATE AculyreDSP)
    add_test(NAME Aculyre.PiezoRouting COMMAND AculyrePiezoRoutingTests)
    set_tests_properties(Aculyre.PiezoRouting PROPERTIES LABELS unit)
    # U11: every control reaches held and new notes after long play.
    find_package(Threads REQUIRED)
    add_executable(AculyreParameterReachTests Tests/ParameterReachTests.cpp)
    target_link_libraries(AculyreParameterReachTests PRIVATE AculyreDSP Threads::Threads)
    add_test(NAME Aculyre.ParameterReach COMMAND AculyreParameterReachTests --reach --jobs 4)
    set_tests_properties(Aculyre.ParameterReach PROPERTIES LABELS unit TIMEOUT 1800)

    # U4: the fuzz target with the shared harness's main, for any compiler. It
    # replays the reproducers committed in Tests/Fixtures/fuzz, then seeded
    # random inputs.
    add_executable(AculyreDspFuzzReplay Tests/DspFuzz.cpp)
    target_link_libraries(AculyreDspFuzzReplay PRIVATE AculyreDSP)
    target_compile_definitions(AculyreDspFuzzReplay PRIVATE PROTOCODUS_FUZZ_MAIN)
    file(GLOB ACULYRE_FUZZ_REPRODUCERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/fuzz/*")
    add_test(NAME Aculyre.DspFuzzReplay
             COMMAND AculyreDspFuzzReplay -seed=1 -runs=200 -max_len=1024 ${ACULYRE_FUZZ_REPRODUCERS})
    set_tests_properties(Aculyre.DspFuzzReplay PROPERTIES LABELS unit TIMEOUT 300)
    if(ACULYRE_BUILD_FUZZER)
        # The fuzz tier: a short libFuzzer run from a fixed seed, growing a
        # corpus in the build tree. A crash's input lands in fuzz-artifacts;
        # once fixed it goes in Tests/Fixtures/fuzz for Aculyre.DspFuzzReplay.
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus" "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts")
        add_test(NAME Aculyre.DspFuzz
                 COMMAND AculyreDspFuzz -max_total_time=120 -seed=1 -max_len=4096 -rss_limit_mb=4096
                         "-artifact_prefix=${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts/"
                         "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus")
        set_tests_properties(Aculyre.DspFuzz PROPERTIES LABELS fuzz TIMEOUT 600)
    endif()

    if(ACULYRE_REQUIRE_PYTHON_TESTS)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    else()
        find_package(Python3 COMPONENTS Interpreter)
        if(NOT Python3_Interpreter_FOUND)
            message(WARNING "Python 3 not found: Aculyre.DemoMidiSources is not registered")
        endif()
    endif()
    if(Python3_Interpreter_FOUND)
        # The committed demo phrases are what the writer writes (G3a).
        add_test(NAME Aculyre.DemoMidiSources
                 COMMAND "${Python3_EXECUTABLE}" -B
                         "${CMAKE_CURRENT_SOURCE_DIR}/Tools/write_preset_midi.py" --check)
        set_tests_properties(Aculyre.DemoMidiSources PROPERTIES LABELS unit)
    endif()

    if(ACULYRE_BUILD_TOOLS)
        # Every key of the keyboard through each preset's engine (Tools/SurveyKeyboard.cpp).
        foreach(preset IN ITEMS 0 1 2)
            add_test(NAME Aculyre.Keyboard${preset}
                     COMMAND AculyreKeyboardSurvey ${preset} 64 0
                             "${CMAKE_BINARY_DIR}/keyboard-${preset}.csv")
            set_tests_properties(Aculyre.Keyboard${preset} PROPERTIES LABELS audio TIMEOUT 600)
        endforeach()
        # Every preset demo and More demo renders finite, audible, under the
        # output ceiling, 8-12 s long and bit-identical on a second render.
        add_test(NAME Aculyre.AudioDemos
                 COMMAND RenderAculyrePresetDemos --jobs 2 --check "${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/midi")
        set_tests_properties(Aculyre.AudioDemos PROPERTIES LABELS audio TIMEOUT 600)
        add_test(NAME Aculyre.MoreDemos
                 COMMAND RenderAculyrePresetDemos --more --jobs 2 --check
                         "${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/midi/more")
        set_tests_properties(Aculyre.MoreDemos PROPERTIES LABELS audio TIMEOUT 600)
        # U7: the demos' realtime factor on one thread against the budget in
        # Tools/RenderPresetDemos.cpp, and U12's worst block, which only warns.
        # Timing means nothing in an unoptimised or instrumented build, so
        # only Release without sanitizers has it.
        if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT ACULYRE_ENABLE_SANITIZERS)
            add_test(NAME Aculyre.CpuBudget
                     COMMAND RenderAculyrePresetDemos --cpu --json "${CMAKE_BINARY_DIR}/cpu.json"
                             "${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/midi")
            set_tests_properties(Aculyre.CpuBudget PROPERTIES LABELS perf RUN_SERIAL TRUE TIMEOUT 900)
        endif()
    endif()
endif()
