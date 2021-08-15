add_contract(defi.logs defi.logs ${CMAKE_CURRENT_SOURCE_DIR}/src/defi.logs.cpp)

target_include_directories(defi.logs
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(defi.logs
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( defi.logs PUBLIC )
