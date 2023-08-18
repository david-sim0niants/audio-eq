find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

set(TARGET_NAME audioeq)
add_library(${TARGET_NAME} SHARED core.cpp filter.cpp filters/low_pass.cpp)

target_link_libraries(${TARGET_NAME} PUBLIC ${PIPEWIRE_LIBRARIES})
target_include_directories(${TARGET_NAME} PUBLIC ${PIPEWIRE_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
target_compile_options(${TARGET_NAME} PUBLIC ${PIPEWIRE_CFLAGS_OTHER})
