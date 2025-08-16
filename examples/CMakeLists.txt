# 示例程序
cmake_minimum_required(VERSION 3.10)

# 基础推理示例
add_executable(basic_inference
    basic_inference.c
)

target_link_libraries(basic_inference
    modyn
    modyn_core
    ${BACKEND_LIBS}
)

# 注释：批量推理和管道推理示例待实现
# add_executable(batch_inference batch_inference.c)
# target_link_libraries(batch_inference modyn modyn_core ${BACKEND_LIBS})

# add_executable(pipeline_inference pipeline_inference.c)
# target_link_libraries(pipeline_inference modyn modyn_core ${BACKEND_LIBS})

# 模型管理示例
add_executable(model_management
    model_management.c
)

target_link_libraries(model_management
    modyn
    modyn_core
    ${BACKEND_LIBS}
)

# 安装示例
install(TARGETS basic_inference model_management
    RUNTIME DESTINATION bin/examples
) 