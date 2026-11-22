add_library(ParallelSchedulingSolver_parallelschedulingsolver)
target_sources(ParallelSchedulingSolver_parallelschedulingsolver PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    solution_builder.cpp
    algorithm_formatter.cpp)
target_include_directories(ParallelSchedulingSolver_parallelschedulingsolver PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_include_directories(ParallelSchedulingSolver_parallelschedulingsolver PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(ParallelSchedulingSolver_parallelschedulingsolver PUBLIC
    OptimizationTools::utils)
add_library(ParallelSchedulingSolver::parallelschedulingsolver ALIAS ParallelSchedulingSolver_parallelschedulingsolver)

add_subdirectory(algorithms)

if(PARALLELSCHEDULINGSOLVER_BUILD_MAIN)
    add_executable(ParallelSchedulingSolver_main)
    target_sources(ParallelSchedulingSolver_main PRIVATE
        main.cpp)
    target_link_libraries(ParallelSchedulingSolver_main PUBLIC
        ParallelSchedulingSolver_parallelschedulingsolver
        ParallelSchedulingSolver_greedy_longest_processing_time
        ParallelSchedulingSolver_greedy_shortest_processing_time
        ParallelSchedulingSolver_greedy_earliest_due_date
        ParallelSchedulingSolver_moore_hodgson
        ParallelSchedulingSolver_assignment_total_flow_time
        ParallelSchedulingSolver_milp_time_indexed
        ParallelSchedulingSolver_local_search
        ParallelSchedulingSolver_milp_assignment
        ParallelSchedulingSolver_milp_positional
        ParallelSchedulingSolver_brute_force
        Boost::program_options
        Boost::filesystem)
    set_target_properties(ParallelSchedulingSolver_main PROPERTIES OUTPUT_NAME "parallelschedulingsolver")
    install(TARGETS ParallelSchedulingSolver_main)
endif()

add_library(ParallelSchedulingSolver_generator)
target_sources(ParallelSchedulingSolver_generator PRIVATE
    generator.cpp)
target_link_libraries(ParallelSchedulingSolver_generator PUBLIC
    ParallelSchedulingSolver_parallelschedulingsolver)
add_library(ParallelSchedulingSolver::generator ALIAS ParallelSchedulingSolver_generator)

if(PARALLELSCHEDULINGSOLVER_BUILD_MAIN)
    add_executable(ParallelSchedulingSolver_generator_main)
    target_sources(ParallelSchedulingSolver_generator_main PRIVATE
        generator_main.cpp)
    target_link_libraries(ParallelSchedulingSolver_generator_main PUBLIC
        ParallelSchedulingSolver_generator
        Boost::program_options)
    set_target_properties(ParallelSchedulingSolver_generator_main PROPERTIES OUTPUT_NAME "parallelschedulingsolver_generator")
    install(TARGETS ParallelSchedulingSolver_generator_main)
endif()
