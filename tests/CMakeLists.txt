# 单元测试
cmake_minimum_required(VERSION 3.10)

# 查找测试框架
find_package(PkgConfig QUIET)

# 内存池测试
add_executable(test_memory_pool
    test_memory_pool.c
)

target_link_libraries(test_memory_pool
    modyn_core
    Threads::Threads
)

# 张量测试
add_executable(test_tensor
    test_tensor.c
)

target_link_libraries(test_tensor
    modyn_core
    Threads::Threads
)

# 注释：模型管理器和推理引擎测试待实现
# add_executable(test_model_manager test_model_manager.c)
# target_link_libraries(test_model_manager modyn modyn_core ${BACKEND_LIBS} Threads::Threads)

# add_executable(test_inference_engine test_inference_engine.c)
# target_link_libraries(test_inference_engine modyn modyn_core ${BACKEND_LIBS} Threads::Threads)

# 集成测试
add_executable(integration_test
    integration_test.c
)

target_link_libraries(integration_test
    modyn
    modyn_core
    ${BACKEND_LIBS}
    Threads::Threads
)

# 注册测试
add_test(NAME memory_pool_test COMMAND test_memory_pool)
add_test(NAME tensor_test COMMAND test_tensor)
add_test(NAME model_manager_test COMMAND test_model_manager)
add_test(NAME inference_engine_test COMMAND test_inference_engine)
add_test(NAME integration_test COMMAND integration_test)

# 设置测试属性
set_tests_properties(memory_pool_test PROPERTIES TIMEOUT 30)
set_tests_properties(tensor_test PROPERTIES TIMEOUT 30)
set_tests_properties(model_manager_test PROPERTIES TIMEOUT 60)
set_tests_properties(inference_engine_test PROPERTIES TIMEOUT 30)
set_tests_properties(integration_test PROPERTIES TIMEOUT 120)

# 安装测试
install(TARGETS test_memory_pool test_tensor integration_test
    RUNTIME DESTINATION bin/tests
) 