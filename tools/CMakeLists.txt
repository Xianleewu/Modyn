# 工具程序
cmake_minimum_required(VERSION 3.10)

# 添加子目录
add_subdirectory(benchmark)
add_subdirectory(model_converter)

# 注释：模型信息工具和后端测试工具待实现
# add_executable(model_info model_info.c)
# target_link_libraries(model_info modyn modyn_core ${BACKEND_LIBS})

# add_executable(backend_test backend_test.c) 
# target_link_libraries(backend_test modyn modyn_core ${BACKEND_LIBS})

# 安装工具（注释掉未实现的目标）
# install(TARGETS model_info backend_test
#     RUNTIME DESTINATION bin/tools
# ) 