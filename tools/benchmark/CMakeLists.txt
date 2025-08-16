# 性能测试工具
cmake_minimum_required(VERSION 3.10)

set(BENCHMARK_SOURCES
    benchmark_tool.c
    benchmark_utils.c
)

add_executable(benchmark_tool ${BENCHMARK_SOURCES})

target_include_directories(benchmark_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(benchmark_tool
    modyn
    modyn_core
    modyn_dummy
    ${BACKEND_LIBS}
    Threads::Threads
    m
)

# 添加时间测量库
find_library(RT_LIB rt)
if(RT_LIB)
    target_link_libraries(benchmark_tool ${RT_LIB})
endif()

# 安装
install(TARGETS benchmark_tool
    RUNTIME DESTINATION bin/tools
) 