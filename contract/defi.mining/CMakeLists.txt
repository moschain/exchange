add_contract(defi.mining defi.mining ${CMAKE_CURRENT_SOURCE_DIR}/src/defi.mining.cpp)

target_include_directories(defi.mining
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(defi.mining
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( defi.mining PUBLIC )
