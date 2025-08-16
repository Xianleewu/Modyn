# REST API 模块
cmake_minimum_required(VERSION 3.10)

set(API_SOURCES
    rest_server.c
)

# 可选的 gRPC 支持
if(ENABLE_GRPC)
    list(APPEND API_SOURCES grpc_server.c)
endif()

add_library(modyn_api STATIC ${API_SOURCES})

target_include_directories(modyn_api PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(modyn_api
    modyn_core
    Threads::Threads
)

# API 服务器可执行文件
add_executable(modyn_api_server
    api_server_main.c
)

target_link_libraries(modyn_api_server
    modyn
    modyn_core
    modyn_api
    ${BACKEND_LIBS}
    Threads::Threads
)

# 安装
install(TARGETS modyn_api modyn_api_server
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
) 