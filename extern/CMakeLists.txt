# Enable FetchContent.
include(FetchContent)

# Fetch boost.
if(PARALLELSCHEDULINGSOLVER_BUILD_MAIN OR PARALLELSCHEDULINGSOLVER_BUILD_TEST)
    set(BOOST_INCLUDE_LIBRARIES thread filesystem system program_options dynamic_bitset)
    set(BOOST_ENABLE_CMAKE ON)
    FetchContent_Declare(
            Boost
            URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
            EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(Boost)
endif()

# Fetch googletest.
if(PARALLELSCHEDULINGSOLVER_BUILD_TEST)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip)
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF)
    FetchContent_MakeAvailable(googletest)
endif()

# Fetch fontanf/optimizationtools.
set(OPTIMIZATIONTOOLS_BUILD_TEST OFF)
FetchContent_Declare(
    optimizationtools
    GIT_REPOSITORY https://github.com/fontanf/optimizationtools.git
    GIT_TAG 2ce692776c181c4b5b5c6235243ce1a1f31ccd4c
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../optimizationtools/"
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(optimizationtools)

# Fetch fontanf/mathoptsolverscmake.
if(PARALLELSCHEDULINGSOLVER_USE_CLP)
    set(MATHOPTSOLVERSCMAKE_USE_CLP ON)
endif()
if(PARALLELSCHEDULINGSOLVER_USE_CBC)
    set(MATHOPTSOLVERSCMAKE_USE_CBC ON)
endif()
if(PARALLELSCHEDULINGSOLVER_USE_HIGHS)
    set(MATHOPTSOLVERSCMAKE_USE_HIGHS ON)
endif()
if(PARALLELSCHEDULINGSOLVER_USE_XPRESS)
    set(MATHOPTSOLVERSCMAKE_USE_XPRESS ON)
endif()
FetchContent_Declare(
    mathoptsolverscmake
    GIT_REPOSITORY https://github.com/fontanf/mathoptsolverscmake.git
    GIT_TAG 07c3651b54cdbc32646610b37fe732900a12fb2c
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../mathoptsolverscmake/"
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(mathoptsolverscmake)

# Fetch fontanf/columngenerationsolver.
set(COLUMNGENERATIONSOLVER_BUILD_EXAMPLES OFF)
set(COLUMNGENERATIONSOLVER_BUILD_TEST OFF)
if(PARALLELSCHEDULINGSOLVER_USE_CLP)
    set(COLUMNGENERATIONSOLVER_USE_CLP ON)
endif()
if(PARALLELSCHEDULINGSOLVER_USE_HIGHS)
    set(COLUMNGENERATIONSOLVER_USE_HIGHS ON)
endif()
if(PARALLELSCHEDULINGSOLVER_USE_XPRESS)
    set(COLUMNGENERATIONSOLVER_USE_XPRESS ON)
endif()
FetchContent_Declare(
    columngenerationsolver
    GIT_REPOSITORY https://github.com/fontanf/columngenerationsolver.git
    GIT_TAG 704f33920c57f435fdd9843ba3189b82caeb3e33
    #SOURCE_DIR "${PROJECT_SOURCE_DIR}/../columngenerationsolver/"
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(columngenerationsolver)
