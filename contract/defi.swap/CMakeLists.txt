add_contract(defi.swap defi.swap ${CMAKE_CURRENT_SOURCE_DIR}/src/defi.swap.cpp)

target_include_directories(defi.swap
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(defi.swap
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( defi.swap PUBLIC )
