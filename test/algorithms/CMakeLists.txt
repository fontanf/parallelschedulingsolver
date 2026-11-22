add_library(ParallelSchedulingSolver_tests)
target_sources(ParallelSchedulingSolver_tests PRIVATE
    tests.cpp)
target_link_libraries(ParallelSchedulingSolver_tests PUBLIC
    ParallelSchedulingSolver_parallelschedulingsolver
    Boost::filesystem
    GTest::gtest_main)
add_library(ParallelSchedulingSolver::tests ALIAS ParallelSchedulingSolver_tests)

add_executable(ParallelSchedulingSolver_greedy_longest_processing_time_test)
target_sources(ParallelSchedulingSolver_greedy_longest_processing_time_test PRIVATE
    greedy_longest_processing_time_test.txt
    greedy_longest_processing_time_test.cpp)
target_link_libraries(ParallelSchedulingSolver_greedy_longest_processing_time_test
    ParallelSchedulingSolver_tests
    ParallelSchedulingSolver_greedy_longest_processing_time
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_greedy_longest_processing_time_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTIES TIMEOUT 3600)

add_executable(ParallelSchedulingSolver_greedy_shortest_processing_time_test)
target_sources(ParallelSchedulingSolver_greedy_shortest_processing_time_test PRIVATE
    greedy_shortest_processing_time_test.txt
    greedy_shortest_processing_time_test.cpp)
target_link_libraries(ParallelSchedulingSolver_greedy_shortest_processing_time_test
    ParallelSchedulingSolver_tests
    ParallelSchedulingSolver_greedy_shortest_processing_time
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_greedy_shortest_processing_time_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTIES TIMEOUT 3600)

add_executable(ParallelSchedulingSolver_greedy_earliest_due_date_test)
target_sources(ParallelSchedulingSolver_greedy_earliest_due_date_test PRIVATE
    greedy_earliest_due_date_test.txt
    greedy_earliest_due_date_test.cpp)
target_link_libraries(ParallelSchedulingSolver_greedy_earliest_due_date_test
    ParallelSchedulingSolver_tests
    ParallelSchedulingSolver_greedy_earliest_due_date
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_greedy_earliest_due_date_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTIES TIMEOUT 3600)

add_executable(ParallelSchedulingSolver_moore_hodgson_test)
target_sources(ParallelSchedulingSolver_moore_hodgson_test PRIVATE
    moore_hodgson_test.txt
    moore_hodgson_test.cpp)
target_link_libraries(ParallelSchedulingSolver_moore_hodgson_test
    ParallelSchedulingSolver_tests
    ParallelSchedulingSolver_moore_hodgson
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_moore_hodgson_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTIES TIMEOUT 3600)

add_executable(ParallelSchedulingSolver_local_search_test)
target_sources(ParallelSchedulingSolver_local_search_test PRIVATE
    local_search_test.txt
    local_search_parallel_tardiness_test.txt
    local_search_setup_times_test.txt
    local_search_test.cpp)
target_link_libraries(ParallelSchedulingSolver_local_search_test
    ParallelSchedulingSolver_tests
    ParallelSchedulingSolver_local_search
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_local_search_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTIES TIMEOUT 3600)

add_executable(ParallelSchedulingSolver_assignment_total_flow_time_test)
target_sources(ParallelSchedulingSolver_assignment_total_flow_time_test PRIVATE
    assignment_total_flow_time_test.txt
    assignment_total_flow_time_test.cpp)
target_link_libraries(ParallelSchedulingSolver_assignment_total_flow_time_test
    ParallelSchedulingSolver_tests
    ParallelSchedulingSolver_assignment_total_flow_time
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_assignment_total_flow_time_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTIES TIMEOUT 3600)

if(PARALLELSCHEDULINGSOLVER_USE_HIGHS)
    add_executable(ParallelSchedulingSolver_milp_time_indexed_test)
    target_sources(ParallelSchedulingSolver_milp_time_indexed_test PRIVATE
        milp_time_indexed_test.txt
        milp_time_indexed_test.cpp)
    target_link_libraries(ParallelSchedulingSolver_milp_time_indexed_test
        ParallelSchedulingSolver_tests
        ParallelSchedulingSolver_milp_time_indexed
        GTest::gtest_main)
    gtest_discover_tests(ParallelSchedulingSolver_milp_time_indexed_test
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        PROPERTIES TIMEOUT 3600)

    add_executable(ParallelSchedulingSolver_milp_assignment_test)
    target_sources(ParallelSchedulingSolver_milp_assignment_test PRIVATE
        milp_assignment_makespan_test.txt
        milp_assignment_maximum_lateness_test.txt
        milp_assignment_test.cpp)
    target_link_libraries(ParallelSchedulingSolver_milp_assignment_test
        ParallelSchedulingSolver_tests
        ParallelSchedulingSolver_milp_assignment
        GTest::gtest_main)
    gtest_discover_tests(ParallelSchedulingSolver_milp_assignment_test
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        PROPERTIES TIMEOUT 3600)

    add_executable(ParallelSchedulingSolver_milp_positional_test)
    target_sources(ParallelSchedulingSolver_milp_positional_test PRIVATE
        milp_positional_test.txt
        milp_positional_test.cpp)
    target_link_libraries(ParallelSchedulingSolver_milp_positional_test
        ParallelSchedulingSolver_tests
        ParallelSchedulingSolver_milp_positional
        GTest::gtest_main)
    gtest_discover_tests(ParallelSchedulingSolver_milp_positional_test
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        PROPERTIES TIMEOUT 3600)
endif()

add_executable(ParallelSchedulingSolver_brute_force_test)
target_sources(ParallelSchedulingSolver_brute_force_test PRIVATE
    brute_force_test.txt
    brute_force_test.cpp)
target_link_libraries(ParallelSchedulingSolver_brute_force_test
    ParallelSchedulingSolver_tests
    ParallelSchedulingSolver_brute_force
    GTest::gtest_main)
gtest_discover_tests(ParallelSchedulingSolver_brute_force_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    PROPERTIES TIMEOUT 3600)
