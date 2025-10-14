# This module defines:
#  MLIR_FOUND - True if MLIR is found
#  MLIR_INCLUDE_DIRS - Include directories for MLIR
#  MLIR_LIBRARIES - Libraries to link against
#  MLIR_DEFINITIONS - Compiler definitions
#  MLIR_VERSION - Version of MLIR found

# Find LLVM first (MLIR is part of LLVM)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# MLIR is typically installed alongside LLVM
set(MLIR_DIR "${LLVM_DIR}/../mlir" CACHE PATH "Path to MLIR CMake files")

# Try to find MLIR CMake configuration (for include paths and definitions)
find_package(MLIR CONFIG PATHS ${MLIR_DIR} NO_DEFAULT_PATH QUIET)

# Set include directories
if(MLIR_FOUND AND MLIR_INCLUDE_DIRS)
    message(STATUS "Using MLIR include dirs from config: ${MLIR_INCLUDE_DIRS}")
else()
    set(MLIR_INCLUDE_DIRS "${LLVM_INCLUDE_DIRS}")
    message(STATUS "Using LLVM include dirs for MLIR: ${MLIR_INCLUDE_DIRS}")
endif()

# Manually find MLIR libraries (the config doesn't provide them)
# Try to find the unified MLIR library (libMLIR.dylib or libMLIR.so)
find_library(MLIR_LIBRARY
    NAMES MLIR
    PATHS ${LLVM_LIBRARY_DIRS}
    NO_DEFAULT_PATH
)

if(MLIR_LIBRARY)
    message(STATUS "Found unified MLIR library: ${MLIR_LIBRARY}")

    # Also find the unified LLVM library (MLIR depends on it)
    find_library(LLVM_LIBRARY
        NAMES LLVM
        PATHS ${LLVM_LIBRARY_DIRS}
        NO_DEFAULT_PATH
    )

    # Find MLIR ExecutionEngine (needed for JIT compilation)
    find_library(MLIR_EXECUTION_ENGINE_LIBRARY
        NAMES MLIRExecutionEngine
        PATHS ${LLVM_LIBRARY_DIRS}
        NO_DEFAULT_PATH
    )

    if(LLVM_LIBRARY)
        message(STATUS "Found unified LLVM library: ${LLVM_LIBRARY}")
        set(MLIR_LIBRARIES ${MLIR_LIBRARY} ${LLVM_LIBRARY})

        if(MLIR_EXECUTION_ENGINE_LIBRARY)
            message(STATUS "Found MLIR ExecutionEngine: ${MLIR_EXECUTION_ENGINE_LIBRARY}")
            list(APPEND MLIR_LIBRARIES ${MLIR_EXECUTION_ENGINE_LIBRARY})
        endif()
    else()
        # Fallback to LLVM_LIBRARIES from config if available
        set(MLIR_LIBRARIES ${MLIR_LIBRARY} ${LLVM_LIBRARIES})
    endif()

    set(MLIR_FOUND TRUE)
else()
    message(FATAL_ERROR "Could not find MLIR library. Please install LLVM with MLIR support.")
endif()

if(MLIR_FOUND)
    set(MLIR_DEFINITIONS ${LLVM_DEFINITIONS})
    set(MLIR_VERSION ${LLVM_PACKAGE_VERSION})

    message(STATUS "MLIR configuration complete")
    message(STATUS "  Version: ${MLIR_VERSION}")
    message(STATUS "  Include dirs: ${MLIR_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${MLIR_LIBRARIES}")
else()
    message(FATAL_ERROR "MLIR not found. Please install LLVM with MLIR support.")
endif()

# Mark variables as advanced
mark_as_advanced(
    MLIR_DIR
    MLIR_INCLUDE_DIRS
    MLIR_LIBRARIES
    MLIR_DEFINITIONS
    MLIR_VERSION
)
