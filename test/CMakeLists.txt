enable_testing()

include(GoogleTest)

add_subdirectory(algorithms)

add_executable(ParallelSchedulingSolver_instance_test)
target_sources(ParallelSchedulingSolver_instance_test PRIVATE
    instance_test.cpp)
target_link_libraries(ParallelSchedulingSolver_instance_test
    ParallelSchedulingSolver_parallelschedulingsolver
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_instance_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
