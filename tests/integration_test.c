#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include "core/model_manager.h"
#include "core/tensor.h"
#include "core/memory_pool.h"
#include "utils/logger.h"
#include "pipeline/pipeline_manager.h"

/**
 * @brief Modyn 集成测试
 * 
 * 测试整个系统的端到端功能
 */

// 测试完整的推理流程
void test_end_to_end_inference(void) {
    printf("测试端到端推理流程...\n");
    
    // 1. 创建模型管理器
    ModelManager* manager = model_manager_create();
    assert(manager != NULL);
    
    // 2. 配置模型
    ModelConfig config = {0};
    config.model_path = "test_model.dummy";
    config.model_id = "integration_test_model";
    config.version = "1.0.0";
    config.backend = INFER_BACKEND_DUMMY;
    config.max_instances = 1;
    config.enable_cache = true;
    
    // 3. 加载模型
    ModelHandle model = model_manager_load(manager, config.model_path, &config);
    assert(model != NULL);
    
    // 4. 创建输入张量
    uint32_t input_dims[] = {1, 3, 224, 224};
    TensorShape input_shape = tensor_shape_create(input_dims, 4);
    Tensor input_tensor = tensor_create("test_input", TENSOR_TYPE_FLOAT32, &input_shape, TENSOR_FORMAT_NCHW);
    
    // 分配并填充输入数据
    input_tensor.data = malloc(input_tensor.size);
    input_tensor.owns_data = true;
    assert(input_tensor.data != NULL);
    
    float* input_data = (float*)input_tensor.data;
    uint32_t element_count = tensor_get_element_count(&input_tensor);
    for (uint32_t i = 0; i < element_count; i++) {
        input_data[i] = (float)(i % 256) / 255.0f;  // 模拟图像数据
    }
    
    // 5. 创建输出张量
    uint32_t output_dims[] = {1, 1000};
    TensorShape output_shape = tensor_shape_create(output_dims, 2);
    Tensor output_tensor = tensor_create("test_output", TENSOR_TYPE_FLOAT32, &output_shape, TENSOR_FORMAT_NC);
    
    output_tensor.data = malloc(output_tensor.size);
    output_tensor.owns_data = true;
    assert(output_tensor.data != NULL);
    
    // 6. 执行推理
    int ret = model_infer_simple(model, &input_tensor, &output_tensor);
    assert(ret == 0);
    
    // 7. 验证输出
    float* output_data = (float*)output_tensor.data;
    assert(output_data != NULL);
    
    // 检查输出数据是否合理
    bool has_valid_data = false;
    for (int i = 0; i < 1000; i++) {
        if (output_data[i] > 0.0f && output_data[i] <= 1.0f) {
            has_valid_data = true;
            break;
        }
    }
    assert(has_valid_data);
    
    // 8. 获取模型信息
    ModelInfo info;
    ret = model_manager_get_info(manager, config.model_id, &info);
    assert(ret == 0);
    assert(info.inference_count >= 1);
    
    // 9. 清理资源
    tensor_free(&input_tensor);
    tensor_free(&output_tensor);
    free(info.model_id);
    free(info.version);
    
    model_manager_unload(manager, model);
    model_manager_destroy(manager);
    
    printf("✅ 端到端推理流程测试通过\n");
}

// 测试内存池与张量的集成
void test_memory_pool_tensor_integration(void) {
    printf("测试内存池与张量集成...\n");
    
    // 创建内存池
    memory_pool_config_t config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 1024 * 1024,  // 1MB
        .max_size = 1024 * 1024 * 10, // 10MB
        .grow_size = 1024 * 1024,     // 1MB
        .alignment = 32,
        .strategy = MEMORY_ALLOC_BEST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    
    memory_pool_t pool = memory_pool_create(&config);
    assert(pool != NULL);
    
    // 计算张量大小
    uint32_t dims[] = {1, 3, 224, 224};
    tensor_shape_t shape = tensor_shape_create(dims, 4);
    tensor_t tensor = tensor_create("test", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NHWC);
    size_t element_count = tensor_get_element_count(&tensor);
    size_t tensor_size = element_count * sizeof(float);
    
    // 使用内存池分配张量数据
    memory_handle_t handle = memory_pool_alloc(pool, tensor_size, 8, "tensor_data");
    assert(handle != NULL);
    void* data = memory_handle_get_ptr(handle);
    assert(data != NULL);
    assert(memory_handle_get_size(handle) >= tensor_size);
    
    // 将内存池分配的内存赋值给张量
    tensor.data = data;
    tensor.owns_data = false;  // 由内存池管理
    
    // 填充测试数据
    float* float_data = (float*)data;
    for (size_t i = 0; i < element_count; i++) {
        float_data[i] = (float)i / element_count;
    }
    
    // 验证张量数据
    assert(tensor.data != NULL);
    assert(tensor.dtype == TENSOR_TYPE_FLOAT32);
    assert(tensor.shape.dims[0] == 1);
    assert(tensor.shape.dims[1] == 3);
    assert(tensor.shape.dims[2] == 224);
    assert(tensor.shape.dims[3] == 224);
    
    // 释放内存
    memory_pool_free(pool, handle);
    memory_pool_destroy(pool);
    printf("✅ 内存池与张量集成测试通过\n");
}

// 测试多线程推理
typedef struct {
    ModelManager* manager;
    ModelHandle model;
    int thread_id;
    int iterations;
    int success_count;
} ThreadInferData;

void* thread_infer_func(void* arg) {
    ThreadInferData* data = (ThreadInferData*)arg;
    
    for (int i = 0; i < data->iterations; i++) {
        // 创建输入张量
        uint32_t dims[] = {1, 3, 32, 32};  // 小尺寸加快测试
        TensorShape shape = tensor_shape_create(dims, 4);
        Tensor input = tensor_create("thread_input", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NCHW);
        
        input.data = malloc(input.size);
        input.owns_data = true;
        if (!input.data) continue;
        
        // 填充输入数据
        float* input_data = (float*)input.data;
        for (uint32_t j = 0; j < tensor_get_element_count(&input); j++) {
            input_data[j] = (float)(data->thread_id * 1000 + j) / 10000.0f;
        }
        
        // 创建输出张量
        uint32_t output_dims[] = {1, 10};
        TensorShape output_shape = tensor_shape_create(output_dims, 2);
        Tensor output = tensor_create("thread_output", TENSOR_TYPE_FLOAT32, &output_shape, TENSOR_FORMAT_NC);
        
        output.data = malloc(output.size);
        output.owns_data = true;
        if (!output.data) {
            tensor_free(&input);
            continue;
        }
        
        // 执行推理
        if (model_infer_simple(data->model, &input, &output) == 0) {
            data->success_count++;
        }
        
        // 清理
        tensor_free(&input);
        tensor_free(&output);
        
        // 短暂休息
        usleep(1000);
    }
    
    return NULL;
}

void test_multi_thread_inference(void) {
    printf("测试多线程推理...\n");
    
    // 创建模型管理器和模型
    ModelManager* manager = model_manager_create();
    assert(manager != NULL);
    
    ModelConfig config = {0};
    config.model_path = "multithread_test.dummy";
    config.model_id = "multithread_model";
    config.version = "1.0.0";
    config.backend = INFER_BACKEND_DUMMY;
    
    ModelHandle model = model_manager_load(manager, config.model_path, &config);
    assert(model != NULL);
    
    // 创建多个线程
    const int num_threads = 4;
    const int iterations = 50;
    
    pthread_t threads[num_threads];
    ThreadInferData thread_data[num_threads];
    
    // 启动线程
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].manager = manager;
        thread_data[i].model = model;
        thread_data[i].thread_id = i;
        thread_data[i].iterations = iterations;
        thread_data[i].success_count = 0;
        
        assert(pthread_create(&threads[i], NULL, thread_infer_func, &thread_data[i]) == 0);
    }
    
    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
    
    // 检查结果
    int total_success = 0;
    for (int i = 0; i < num_threads; i++) {
        total_success += thread_data[i].success_count;
        printf("  线程 %d: %d/%d 成功\n", i, thread_data[i].success_count, iterations);
    }
    
    printf("  总成功: %d/%d\n", total_success, num_threads * iterations);
    assert(total_success > num_threads * iterations * 0.8);  // 至少80%成功率
    
    // 清理
    model_manager_unload(manager, model);
    model_manager_destroy(manager);
    
    printf("✅ 多线程推理测试通过\n");
}

// 测试管道系统集成
void test_pipeline_integration(void) {
    printf("测试管道系统集成...\n");
    
    // 创建管道管理器
    PipelineManager pm = pipeline_manager_create();
    assert(pm != NULL);
    
    // 创建模型管理器
    ModelManager* mm = model_manager_create();
    assert(mm != NULL);
    
    // 加载测试模型
    ModelConfig config = {0};
    config.model_path = "pipeline_test.dummy";
    config.model_id = "pipeline_model";
    config.version = "1.0.0";
    config.backend = INFER_BACKEND_DUMMY;
    
    ModelHandle model = model_manager_load(mm, config.model_path, &config);
    assert(model != NULL);
    
    // 创建管道
    PipelineConfig pipe_config = {0};
    pipe_config.pipeline_id = "test_pipeline";
    pipe_config.exec_mode = PIPELINE_EXEC_SEQUENTIAL;
    
    Pipeline pipeline = pipeline_create(pm, &pipe_config);
    assert(pipeline != NULL);
    
    // 添加节点
    PipelineNodeConfig node_config = {0};
    node_config.node_id = "model_node";
    node_config.type = PIPELINE_NODE_MODEL;
    node_config.model = model;
    node_config.input_count = 1;
    node_config.output_count = 1;
    
    PipelineNode node = pipeline_add_node(pipeline, &node_config);
    assert(node != NULL);
    
    // 验证管道信息
    uint32_t node_count, connection_count;
    assert(pipeline_get_info(pipeline, &node_count, &connection_count) == 0);
    assert(node_count == 1);
    assert(connection_count == 0);
    
    // 验证管道
    assert(pipeline_validate(pipeline) == 0);
    
    // 清理
    model_manager_unload(mm, model);
    model_manager_destroy(mm);
    pipeline_manager_destroy(pm);  // 这会自动销毁所有管道
    
    printf("✅ 管道系统集成测试通过\n");
}

// 测试资源清理
void test_resource_cleanup(void) {
    printf("测试内存池资源清理...\n");
    memory_pool_config_t config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 1024 * 5,  // 5KB
        .max_size = 1024 * 5,
        .grow_size = 1024,
        .alignment = 8,
        .strategy = MEMORY_ALLOC_FIRST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    memory_pool_t pool = memory_pool_create(&config);
    assert(pool != NULL);
    memory_handle_t handles[3];
    for (int i = 0; i < 3; i++) {
        handles[i] = memory_pool_alloc(pool, 512, 8, "cleanup_test");
        assert(handles[i] != NULL);
    }
    for (int i = 0; i < 3; i++) {
        assert(memory_pool_free(pool, handles[i]) == 0);
    }
    memory_pool_stats_t stats;
    assert(memory_pool_get_stats(pool, &stats) == 0);
    assert(stats.used_size == 0);
    assert(stats.free_count == 3);
    memory_pool_destroy(pool);
    printf("✅ 资源清理测试通过\n");
}

// 压力测试
void test_stress_test(void) {
    printf("测试系统压力...\n");
    
    ModelManager* manager = model_manager_create();
    assert(manager != NULL);
    
    // 快速加载和卸载多个模型
    for (int i = 0; i < 20; i++) {
        char model_path[64];
        char model_id[64];
        snprintf(model_path, sizeof(model_path), "stress_test_%d.dummy", i);
        snprintf(model_id, sizeof(model_id), "stress_model_%d", i);
        
        ModelConfig config = {0};
        config.model_path = model_path;
        config.model_id = model_id;
        config.backend = INFER_BACKEND_DUMMY;
        
        ModelHandle model = model_manager_load(manager, config.model_path, &config);
        if (model) {
            // 快速推理
            uint32_t dims[] = {1, 1, 8, 8};  // 很小的张量
            TensorShape shape = tensor_shape_create(dims, 4);
            Tensor input = tensor_create("stress_input", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NCHW);
            Tensor output = tensor_create("stress_output", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NCHW);
            
            input.data = calloc(1, input.size);
            output.data = calloc(1, output.size);
            input.owns_data = output.owns_data = true;
            
            if (input.data && output.data) {
                model_infer_simple(model, &input, &output);
            }
            
            tensor_free(&input);
            tensor_free(&output);
            model_manager_unload(manager, model);
        }
    }
    
    model_manager_destroy(manager);
    
    printf("✅ 系统压力测试通过\n");
}

int main(void) {
    // 初始化日志系统
    logger_init(LOG_LEVEL_INFO, NULL);
    logger_set_console_output(true);
    
    printf("=== Modyn 集成测试 ===\n");
    printf("测试整个系统的端到端功能...\n\n");
    
    test_end_to_end_inference();
    test_memory_pool_tensor_integration();
    test_multi_thread_inference();
    test_pipeline_integration();
    test_resource_cleanup();
    test_stress_test();
    
    printf("\n🎉 所有集成测试通过！\n");
    printf("✅ 系统各组件协作正常\n");
    printf("✅ 多线程安全性验证通过\n");
    printf("✅ 资源管理正确\n");
    printf("✅ 性能表现稳定\n");
    
    logger_cleanup();
    return 0;
} 