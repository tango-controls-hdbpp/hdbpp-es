# Functions and Pre-build  -----------------------------------

# Stop messy in source builds
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)

if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
    message( FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt." )
endif()

# Start Build Config -----------------------------------
cmake_minimum_required(VERSION 3.8)
set(CMAKE_SKIP_RPATH true)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE ON)

# Output name for the final binary
set(HDBPP_ES_NAME "hdb++es-srv")

# Versioning
set(VERSION_MAJOR "2")
set(VERSION_MINOR "0")
set(VERSION_PATCH "0")
set(VERSION_METADATA "")
set(VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

# Add any include paths from the command line
list(APPEND INCLUDE_PATHS ${CMAKE_INCLUDE_PATH})
list(APPEND INCLUDE_PATHS ${CMAKE_SOURCE_DIR})
list(APPEND LIBRARY_PATHS ${CMAKE_LIBRARY_PATH})

# Start the project
project(hdbpp_es VERSION ${VERSION_STRING} LANGUAGES CXX)

set(FETCH_LIBHDBPP_TAG "master" CACHE STRING "Libhdbpp branch/tag to clone 'master'")
option(FETCH_LIBHDBPP "Download and build using a local copy of libhdb++" OFF)
option(FETCH_LIBHDBPP_TAG "When FETCH_LIBHDBPP is enabled, this is the tag fetch, default is 'master'" "master")
option(ENABLE_CLANG "Enable clang code and layout analysis" OFF)

# arch install definitions
include(GNUInstallDirs)

message(STATUS "Searching for libraries...")

# Variable to contain a list of all the libs we depend on
set(HDBPP_ES_LIBRARIES) 

# allow pkg-config to search the CMAKE_PREFIX_PATH 
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
list(APPEND CMAKE_PREFIX_PATH "/usr")

# Find Dependencies  -----------------------------------
if(FETCH_LIBHDBPP)
    set(FETCH_PROJECT_NAME "libhdb++")
    set(FETCH_REPOSITORY_ADDRESS "https://github.com/tango-controls-hdbpp/libhdbpp.git")
    set(FETCH_REPOSITORY_TAG ${FETCH_LIBHDBPP_TAG})
 
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchExternal.cmake.in ${CMAKE_BINARY_DIR}/${FETCH_PROJECT_NAME}-download/CMakeLists.txt)

    execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${FETCH_PROJECT_NAME}-download" )

    execute_process(COMMAND "${CMAKE_COMMAND}" --build .
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${FETCH_PROJECT_NAME}-download" )

    add_subdirectory("${CMAKE_BINARY_DIR}/${FETCH_PROJECT_NAME}-src" "${CMAKE_BINARY_DIR}/${FETCH_PROJECT_NAME}-build" EXCLUDE_FROM_ALL)

    # For ease of use, we set the libhdb++ output to our projects build directory,
    # making linking and running easy
    set_target_properties(libhdbpp_shared_library
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    set(BACKEND_LIBRARY_TARGET "libhdbpp_shared_library")

elseif(NOT LIBHDBPP_PROJECT_BUILD AND NOT FETCH_LIBHDBPP)
    find_library(LIBHDBPP_LIB_SEARCH libhdb++.so.2 PATHS LIBRARY_PATHS)
    message(STATUS "Found: ${LIBHDBPP_LIB_SEARCH}")
endif()

# split the generator expression out to here so we can make it more
# readable
string(CONCAT HDBPP_LIB 
    "$<IF:"
        "$<OR:"
            "$<BOOL:${LIBHDBPP_PROJECT_BUILD}>,"
            "$<BOOL:${FETCH_LIBHDBPP}>"
        ">,"
        "${BACKEND_LIBRARY_TARGET},${LIBHDBPP_LIB_SEARCH}"
    ">")

# Find tango if it has not already been found. Returns an interface library
# called TangoInterfaceLibrary
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Tango)

# Code Analysis -----------------------------------
if(ENABLE_CLANG)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # To find clang, find_program will search your PATH environment variable.
    # Ensure if you have a non-standard clang install, that it has been added
    # to your path.
    find_program(CLANG_TIDY_EXE 
        NAMES "clang-tidy" 
        DOC "Path to clang-tidy executable")

    if(NOT CLANG_TIDY_EXE)
        message(STATUS "clang-tidy not found.")
    else(NOT CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif(NOT CLANG_TIDY_EXE)
endif(ENABLE_CLANG)

# Source  -----------------------------------

add_subdirectory(src)

# Build Targets  -----------------------------------

# Executable --------
add_executable(hdbpp_es ${SRC_FILES})

target_link_libraries(hdbpp_es 
    PRIVATE 
        TangoInterfaceLibrary 
        ${HDBPP_LIB})

target_include_directories(hdbpp_es 
    PRIVATE 
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        ${INCLUDE_PATHS}
        "${PROJECT_BINARY_DIR}")

set_target_properties(hdbpp_es 
    PROPERTIES 
        OUTPUT_NAME ${HDBPP_ES_NAME}
        LINK_FLAGS "-Wl,--no-undefined"
        CXX_STANDARD 11)

if(DO_CLANG_TIDY)
    set_target_properties(hdbpp_es 
        PROPERTIES 
            CXX_CLANG_TIDY ${DO_CLANG_TIDY})
endif(DO_CLANG_TIDY)

target_compile_options(hdbpp_es 
    PRIVATE "$<$<CONFIG:DEBUG>:-g>")

# Install Config -----------------------------------
install(
    TARGETS hdbpp_es
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

message(STATUS "Configured hdbpp-es project")