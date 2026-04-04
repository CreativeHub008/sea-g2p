cmake_minimum_required(VERSION 3.16)
project(sea_g2p VERSION 0.7.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Options ──────────────────────────────────────────────────────────────────
option(SEA_G2P_FETCH_PCRE2   "Fetch PCRE2 via FetchContent if not found on system" ON)
option(SEA_G2P_BUILD_SHARED  "Build as shared library instead of static"           OFF)
option(SEA_G2P_ENABLE_ICU    "Use ICU for NFC Unicode normalization (recommended)"  ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    set(SEA_G2P_IS_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})
else()
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        set(SEA_G2P_IS_TOP_LEVEL ON)
    else()
        set(SEA_G2P_IS_TOP_LEVEL OFF)
    endif()
endif()

option(SEA_G2P_BUILD_EXAMPLES "Build example programs" ${SEA_G2P_IS_TOP_LEVEL})
option(SEA_G2P_BUILD_TESTS    "Build test suite"       ${SEA_G2P_IS_TOP_LEVEL})

# ── PCRE2 (required) ─────────────────────────────────────────────────────────
find_package(PCRE2 CONFIG QUIET COMPONENTS 8BIT)

if(NOT PCRE2_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PCRE2 QUIET libpcre2-8)
    endif()
endif()

if(NOT PCRE2_FOUND)
    if(SEA_G2P_FETCH_PCRE2)
        include(FetchContent)
        FetchContent_Declare(
            pcre2
            GIT_REPOSITORY https://github.com/PCRE2Project/pcre2.git
            GIT_TAG        pcre2-10.45
            GIT_SHALLOW    TRUE
        )
        set(PCRE2_BUILD_PCRE2_8     ON  CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_PCRE2_16    OFF CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_PCRE2_32    OFF CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_TESTS       OFF CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_PCRE2GREP   OFF CACHE BOOL "" FORCE)
        set(PCRE2_SUPPORT_UNICODE   ON  CACHE BOOL "" FORCE)
        set(PCRE2_SUPPORT_JIT       ON  CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(pcre2)
        
        if(TARGET pcre2-8-static)
            set(_PCRE2_TARGET pcre2-8-static)
        else()
            set(_PCRE2_TARGET pcre2-8)
        endif()
    else()
        message(FATAL_ERROR
            "PCRE2 not found. Install libpcre2-dev (Linux) / pcre2 (brew/vcpkg), "
            "or set SEA_G2P_FETCH_PCRE2=ON to download automatically.")
    endif()
else()
    if(TARGET PCRE2::pcre2-8)
        set(_PCRE2_TARGET PCRE2::pcre2-8)
    else()
        set(_PCRE2_TARGET ${PCRE2_LIBRARIES})
    endif()
endif()

# ── ICU (optional, for NFC normalization) ────────────────────────────────────
set(_HAS_ICU FALSE)
if(SEA_G2P_ENABLE_ICU)
    find_package(ICU QUIET COMPONENTS uc)
    if(ICU_FOUND)
        set(_HAS_ICU TRUE)
        message(STATUS "sea_g2p: ICU found — NFC normalization enabled")
    else()
        message(STATUS "sea_g2p: ICU not found — NFC normalization disabled (input assumed NFC)")
    endif()
endif()

# ── Library target ────────────────────────────────────────────────────────────
set(SEA_G2P_SOURCES
    src/regex_wrapper.cpp
    src/resources.cpp
    src/num2vi.cpp
    src/numerical.cpp
    src/datestime.cpp
    src/units.cpp
    src/technical.cpp
    src/misc.cpp
    src/vi_normalizer.cpp
    src/g2p.cpp
)

if(SEA_G2P_BUILD_SHARED)
    add_library(sea_g2p SHARED ${SEA_G2P_SOURCES})
else()
    add_library(sea_g2p STATIC ${SEA_G2P_SOURCES})
endif()

add_library(sea_g2p::sea_g2p ALIAS sea_g2p)
# ── Asset Management ──────────────────────────────────────────────────────────
set(DICT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../python/sea_g2p/sea_g2p.bin")
set_target_properties(sea_g2p PROPERTIES SEA_G2P_DICT_FILE "${DICT_SRC}")

# Helper function for consumers to copy the dictionary to their target directory
function(sea_g2p_add_dictionary target)
    get_target_property(DICT_PATH sea_g2p SEA_G2P_DICT_FILE)
    if(NOT DICT_PATH)
        message(FATAL_ERROR "sea_g2p: SEA_G2P_DICT_FILE property not found on target 'sea_g2p'")
    endif()
    
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DICT_PATH}"
        "$<TARGET_FILE_DIR:${target}>/sea_g2p.bin"
        COMMENT "Copying sea_g2p dictionary to output directory of ${target}"
    )
endfunction()

target_include_directories(sea_g2p
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sea_g2p PRIVATE $<BUILD_INTERFACE:${_PCRE2_TARGET}>)

if(_HAS_ICU)
    target_link_libraries(sea_g2p PRIVATE ICU::uc)
    target_compile_definitions(sea_g2p PRIVATE SEA_G2P_HAS_ICU)
endif()

# Windows: use UTF-8 execution charset and link shell32 for argv conversion
if(WIN32)
    target_compile_options(sea_g2p PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(sea_g2p INTERFACE shell32)
endif()


# ── Installation ──────────────────────────────────────────────────────────────
if(SEA_G2P_IS_TOP_LEVEL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS sea_g2p
        EXPORT sea_g2pTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    if(EXISTS "${DICT_SRC}")
        install(FILES "${DICT_SRC}" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    # Config files
    configure_package_config_file(
        "cmake/sea_g2pConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/sea_g2pConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sea_g2p
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/sea_g2pConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT sea_g2pTargets
        FILE sea_g2pTargets.cmake
        NAMESPACE sea_g2p::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sea_g2p
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/sea_g2pConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/sea_g2pConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sea_g2p
    )
endif()

# ── Examples & Tests ──────────────────────────────────────────────────────────
if(SEA_G2P_BUILD_EXAMPLES)
    add_executable(example_main example_main.cpp)
    target_link_libraries(example_main PRIVATE sea_g2p::sea_g2p)

    add_executable(benchmark benchmark.cpp)
    target_link_libraries(benchmark PRIVATE sea_g2p::sea_g2p)
endif()

if(SEA_G2P_BUILD_TESTS)
    add_executable(test_all tests/test_all.cpp)
    target_link_libraries(test_all PRIVATE sea_g2p::sea_g2p)
endif()
# ── Asset Management (Internal Examples & Tests) ──────────────────────────────
if(EXISTS "${DICT_SRC}")
    # For single-config generators (like Ninja/Unix Makefiles)
    file(COPY "${DICT_SRC}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    
    if(SEA_G2P_BUILD_EXAMPLES)
        sea_g2p_add_dictionary(example_main)
        sea_g2p_add_dictionary(benchmark)
    endif()
    if(SEA_G2P_BUILD_TESTS)
        sea_g2p_add_dictionary(test_all)
    endif()
endif()
