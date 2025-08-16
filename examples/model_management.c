#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core/model_manager.h"
#include "core/inference_engine.h"

/**
 * @brief 模型管理示例
 * 
 * 这个示例演示了如何使用 Modyn 进行模型管理：
 * 1. 创建模型管理器
 * 2. 加载多个模型
 * 3. 查询模型信息
 * 4. 列出所有模型
 * 5. 获取可用后端
 * 6. 管理模型生命周期
 */

void print_separator(void) {
    printf("================================================\n");
}

void print_backend_info(void) {
    printf("🔧 查询可用后端...\n");
    
    InferBackendType backends[10];
    uint32_t count = 10;
    
    int ret = infer_engine_get_available_backends(backends, &count);
    if (ret == 0) {
        printf("✅ 找到 %d 个可用后端:\n", count);
        for (uint32_t i = 0; i < count; i++) {
            const char* name = infer_engine_get_backend_name(backends[i]);
            printf("   %d. %s (ID: %d)\n", i + 1, name, backends[i]);
        }
    } else {
        printf("❌ 获取后端信息失败\n");
    }
}

void print_model_info(ModelManager* manager, const char* model_id) {
    ModelInfo info;
    int ret = model_manager_get_info(manager, model_id, &info);
    
    if (ret == 0) {
        printf("📊 模型信息: %s\n", model_id);
        printf("   模型ID: %s\n", info.model_id);
        printf("   版本: %s\n", info.version);
        printf("   状态: ");
        switch (info.status) {
            case MODEL_STATUS_UNLOADED:
                printf("未加载\n");
                break;
            case MODEL_STATUS_LOADING:
                printf("加载中\n");
                break;
            case MODEL_STATUS_LOADED:
                printf("已加载\n");
                break;
            case MODEL_STATUS_ERROR:
                printf("错误\n");
                break;
            default:
                printf("未知\n");
        }
        printf("   实例数量: %d\n", info.instance_count);
        printf("   内存使用: %lu bytes\n", info.memory_usage);
        printf("   推理次数: %lu\n", info.inference_count);
        printf("   平均延迟: %.2f ms\n", info.avg_latency);
        
        // 释放分配的字符串
        free(info.model_id);
        free(info.version);
    } else {
        printf("❌ 获取模型信息失败: %s\n", model_id);
    }
}

void list_all_models(ModelManager* manager) {
    printf("📋 列出所有模型...\n");
    
    char* model_ids[10];
    uint32_t count = 10;
    
    int ret = model_manager_list(manager, model_ids, &count);
    if (ret == 0) {
        if (count > 0) {
            printf("✅ 找到 %d 个模型:\n", count);
            for (uint32_t i = 0; i < count; i++) {
                printf("   %d. %s\n", i + 1, model_ids[i]);
                free(model_ids[i]); // 释放字符串
            }
        } else {
            printf("ℹ️  没有已加载的模型\n");
        }
    } else {
        printf("❌ 列出模型失败\n");
    }
}

ModelHandle load_test_model(ModelManager* manager, const char* model_id, const char* model_path) {
    printf("📥 加载模型: %s\n", model_id);
    
    ModelConfig config = {0};
    config.model_path = (char*)model_path;
    config.model_id = (char*)model_id;
    config.version = "1.0.0";
    config.backend = INFER_BACKEND_DUMMY; // 使用虚拟后端
    config.max_instances = 1;
    config.enable_cache = true;
    
    ModelHandle model = model_manager_load(manager, model_path, &config);
    if (model) {
        printf("✅ 模型加载成功: %s\n", model_id);
    } else {
        printf("❌ 模型加载失败: %s\n", model_id);
    }
    
    return model;
}

int test_model_inference(ModelHandle model, const char* model_id) {
    printf("🧪 测试模型推理: %s\n", model_id);
    
    // 创建测试输入
    uint32_t input_dims[] = {1, 3, 224, 224};
    TensorShape input_shape = tensor_shape_create(input_dims, 4);
    Tensor input_tensor = tensor_create("input", TENSOR_TYPE_FLOAT32, &input_shape, TENSOR_FORMAT_NCHW);
    
    // 分配内存并填充数据
    input_tensor.data = malloc(input_tensor.size);
    if (!input_tensor.data) {
        printf("❌ 内存分配失败\n");
        return -1;
    }
    
    float* data = (float*)input_tensor.data;
    uint32_t element_count = tensor_get_element_count(&input_tensor);
    for (uint32_t i = 0; i < element_count; i++) {
        data[i] = (float)(rand() % 256) / 255.0f;
    }
    
    // 创建输出张量
    uint32_t output_dims[] = {1, 1000};
    TensorShape output_shape = tensor_shape_create(output_dims, 2);
    Tensor output_tensor = tensor_create("output", TENSOR_TYPE_FLOAT32, &output_shape, TENSOR_FORMAT_NC);
    
    output_tensor.data = malloc(output_tensor.size);
    if (!output_tensor.data) {
        printf("❌ 输出内存分配失败\n");
        tensor_free(&input_tensor);
        return -1;
    }
    
    // 执行推理
    int ret = model_infer_simple(model, &input_tensor, &output_tensor);
    if (ret == 0) {
        printf("✅ 推理成功\n");
    } else {
        printf("❌ 推理失败，错误码: %d\n", ret);
    }
    
    // 清理资源
    tensor_free(&input_tensor);
    tensor_free(&output_tensor);
    
    return ret;
}

int main(int argc, char* argv[]) {
    printf("=== Modyn 模型管理示例 ===\n");
    print_separator();
    
    // 1. 显示后端信息
    print_backend_info();
    print_separator();
    
    // 2. 创建模型管理器
    printf("🚀 创建模型管理器...\n");
    ModelManager* manager = model_manager_create();
    if (!manager) {
        printf("❌ 创建模型管理器失败\n");
        return 1;
    }
    printf("✅ 模型管理器创建成功\n");
    print_separator();
    
    // 3. 加载多个模型
    printf("📦 加载多个模型...\n");
    
    ModelHandle models[3];
    const char* model_ids[] = {"yolo_v5", "resnet_50", "mobilenet_v2"};
    const char* model_paths[] = {"yolo_v5.rknn", "resnet_50.rknn", "mobilenet_v2.rknn"};
    
    for (int i = 0; i < 3; i++) {
        models[i] = load_test_model(manager, model_ids[i], model_paths[i]);
        if (models[i]) {
            printf("✅ 模型 %s 加载成功\n", model_ids[i]);
        } else {
            printf("❌ 模型 %s 加载失败\n", model_ids[i]);
        }
        printf("\n");
    }
    print_separator();
    
    // 4. 列出所有模型
    list_all_models(manager);
    print_separator();
    
    // 5. 查询每个模型的详细信息
    printf("🔍 查询模型详细信息...\n");
    for (int i = 0; i < 3; i++) {
        if (models[i]) {
            print_model_info(manager, model_ids[i]);
            printf("\n");
        }
    }
    print_separator();
    
    // 6. 测试模型推理
    printf("🧪 测试模型推理...\n");
    for (int i = 0; i < 3; i++) {
        if (models[i]) {
            test_model_inference(models[i], model_ids[i]);
            printf("\n");
        }
    }
    print_separator();
    
    // 7. 再次查询模型信息（查看推理统计）
    printf("📊 查看推理统计...\n");
    for (int i = 0; i < 3; i++) {
        if (models[i]) {
            print_model_info(manager, model_ids[i]);
            printf("\n");
        }
    }
    print_separator();
    
    // 8. 测试模型获取
    printf("🔄 测试模型获取...\n");
    for (int i = 0; i < 3; i++) {
        ModelHandle handle = model_manager_get(manager, model_ids[i]);
        if (handle) {
            printf("✅ 成功获取模型: %s\n", model_ids[i]);
            // 这里应该调用 model_manager_unload 来释放获取的句柄
            model_manager_unload(manager, handle);
        } else {
            printf("❌ 获取模型失败: %s\n", model_ids[i]);
        }
    }
    print_separator();
    
    // 9. 卸载模型
    printf("🗑️  卸载模型...\n");
    for (int i = 0; i < 3; i++) {
        if (models[i]) {
            int ret = model_manager_unload(manager, models[i]);
            if (ret == 0) {
                printf("✅ 模型 %s 卸载成功\n", model_ids[i]);
            } else {
                printf("❌ 模型 %s 卸载失败\n", model_ids[i]);
            }
        }
    }
    print_separator();
    
    // 10. 最终检查
    printf("🔍 最终检查...\n");
    list_all_models(manager);
    print_separator();
    
    // 11. 销毁管理器
    printf("🧹 销毁模型管理器...\n");
    model_manager_destroy(manager);
    printf("✅ 模型管理器销毁完成\n");
    
    printf("\n=== 模型管理示例完成 ===\n");
    
    return 0;
} 