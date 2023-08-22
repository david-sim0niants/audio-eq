cmake_minimum_required(VERSION 3.10)
project(audio-eq C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(libaudioeq)

set(TARGET_NAME audioeq_main)
add_executable(${TARGET_NAME} main.cpp)
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME audioeq)
target_link_libraries(${TARGET_NAME} PRIVATE audioeq)

