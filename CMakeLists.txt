cmake_minimum_required(VERSION 3.20)
project(ventpy
    VERSION 0.1.0
    DESCRIPTION "High-performance mine ventilation calculation library"
    LANGUAGES CXX
)

# ============================================================================
# C++20 obligatorio
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json para LSP/IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Header-only core library (interfaz)
# ============================================================================
add_library(ventpy_core INTERFACE)
target_include_directories(ventpy_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(ventpy_core INTERFACE cxx_std_20)

# ============================================================================
# Python bindings (nanobind via scikit-build-core)
# ============================================================================
option(VENTPY_BUILD_PYTHON "Build Python bindings" ON)

if(VENTPY_BUILD_PYTHON)
    # Buscar Python y nanobind
    find_package(Python 3.9
        REQUIRED COMPONENTS Interpreter Development.Module
        OPTIONAL_COMPONENTS Development.SABIModule
    )

    # nanobind: descargado via pip (nanobind es un paquete Python)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE NB_DIR
        RESULT_VARIABLE NB_RESULT
    )

    if(NB_RESULT EQUAL 0 AND EXISTS "${NB_DIR}")
        list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
        find_package(nanobind CONFIG REQUIRED)

        nanobind_add_module(_ventpy_core
            STABLE_ABI
            NB_STATIC
            bindings/bindings.cpp
        )
        target_link_libraries(_ventpy_core PRIVATE ventpy_core)

        # Instalar el módulo en el paquete Python.
        # En Windows el módulo puede instalarse como RUNTIME (.pyd).
        install(TARGETS _ventpy_core
            LIBRARY DESTINATION ventpy
            RUNTIME DESTINATION ventpy
            ARCHIVE DESTINATION ventpy
        )
    else()
        message(FATAL_ERROR
            "nanobind not found, so the Python package would be incomplete.\n"
            "Install with: pip install nanobind"
        )
    endif()
endif()

# ============================================================================
# C++ Tests (Google Test)
# ============================================================================
option(VENTPY_BUILD_TESTS "Build C++ unit tests" OFF)

if(VENTPY_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    # Para Windows: evitar sobreescribir /MD con /MT
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(ventpy_tests
        tests/cpp/test_validation.cpp
        tests/cpp/test_caudal_personal.cpp
        tests/cpp/test_caudal_equipo.cpp
        tests/cpp/test_caudal_explosivos.cpp
        tests/cpp/test_caudal_fugas.cpp
        tests/cpp/test_governor.cpp
        tests/cpp/test_normativa.cpp
        tests/cpp/test_limites_gases.cpp
        tests/cpp/test_cobertura.cpp
        tests/cpp/test_atkinson.cpp
        tests/cpp/test_ducto.cpp
    )
    target_link_libraries(ventpy_tests PRIVATE
        ventpy_core
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(ventpy_tests)
endif()
