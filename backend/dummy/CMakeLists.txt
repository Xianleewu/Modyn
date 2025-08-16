# Dummy Backend for Testing and Debugging
cmake_minimum_required(VERSION 3.10)

set(DUMMY_SOURCES
    dummy_engine.c
)

add_library(modyn_dummy STATIC ${DUMMY_SOURCES})

target_include_directories(modyn_dummy PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(modyn_dummy
    Threads::Threads
)

# 安装
install(TARGETS modyn_dummy
    ARCHIVE DESTINATION lib
) 