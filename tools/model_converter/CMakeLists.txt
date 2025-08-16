# 模型转换工具
cmake_minimum_required(VERSION 3.10)

set(CONVERTER_SOURCES
    model_converter.c
)

add_executable(model_converter ${CONVERTER_SOURCES})

target_include_directories(model_converter PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(model_converter
    modyn
    modyn_core
    ${BACKEND_LIBS}
)

# 安装
install(TARGETS model_converter
    RUNTIME DESTINATION bin/tools
) 