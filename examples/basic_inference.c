#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/model_manager.h"
#include "core/tensor.h"

/**
 * @brief 基础推理示例
 * 
 * 这个示例演示了如何使用 Modyn 进行基础的模型推理：
 * 1. 创建模型管理器
 * 2. 加载模型
 * 3. 准备输入数据
 * 4. 执行推理
 * 5. 处理输出结果
 * 6. 清理资源
 */

void print_usage(const char* program_name) {
    printf("用法: %s <模型文件路径> [图像文件路径]\n", program_name);
    printf("示例: %s model.rknn input.jpg\n", program_name);
    printf("\n");
    printf("如果没有提供图像文件，将使用虚拟数据进行推理\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* model_path = argv[1];
    const char* image_path = argc > 2 ? argv[2] : NULL;
    
    printf("=== Modyn 基础推理示例 ===\n");
    printf("模型文件: %s\n", model_path);
    if (image_path) {
        printf("输入图像: %s\n", image_path);
    } else {
        printf("使用虚拟数据进行推理\n");
    }
    printf("\n");
    
    // 1. 创建模型管理器
    printf("1. 创建模型管理器...\n");
    ModelManager* manager = model_manager_create();
    if (!manager) {
        printf("❌ 创建模型管理器失败\n");
        return 1;
    }
    printf("✅ 模型管理器创建成功\n");
    
    // 2. 加载模型
    printf("\n2. 加载模型...\n");
    ModelConfig config = {0};
    config.model_path = (char*)model_path;
    config.model_id = "test_model";
    config.version = "1.0.0";
    config.backend = INFER_BACKEND_DUMMY; // 使用虚拟后端进行演示
    config.max_instances = 1;
    config.enable_cache = true;
    
    ModelHandle model = model_manager_load(manager, model_path, &config);
    if (!model) {
        printf("❌ 模型加载失败\n");
        model_manager_destroy(manager);
        return 1;
    }
    printf("✅ 模型加载成功\n");
    
    // 3. 准备输入数据
    printf("\n3. 准备输入数据...\n");
    
    // 创建输入张量 (1, 3, 224, 224)
    uint32_t input_dims[] = {1, 3, 224, 224};
    TensorShape input_shape = tensor_shape_create(input_dims, 4);
    
    Tensor input_tensor;
    if (image_path) {
        // 从图像文件创建张量
        input_tensor = prepare_tensor_from_image(image_path, &input_shape, TENSOR_FORMAT_NCHW);
    } else {
        // 创建虚拟输入数据
        input_tensor = tensor_create("input", TENSOR_TYPE_FLOAT32, &input_shape, TENSOR_FORMAT_NCHW);
        
        // 分配内存并填充虚拟数据
        input_tensor.data = malloc(input_tensor.size);
        if (!input_tensor.data) {
            printf("❌ 内存分配失败\n");
            model_manager_unload(manager, model);
            model_manager_destroy(manager);
            return 1;
        }
        
        // 填充随机数据
        float* data = (float*)input_tensor.data;
        uint32_t element_count = tensor_get_element_count(&input_tensor);
        for (uint32_t i = 0; i < element_count; i++) {
            data[i] = (float)(rand() % 256) / 255.0f; // 模拟图像数据 [0, 1]
        }
    }
    
    printf("✅ 输入数据准备完成\n");
    printf("   输入形状: [%d, %d, %d, %d]\n", 
           input_tensor.shape.dims[0], input_tensor.shape.dims[1], 
           input_tensor.shape.dims[2], input_tensor.shape.dims[3]);
    
    // 4. 准备输出张量
    printf("\n4. 准备输出数据...\n");
    
    uint32_t output_dims[] = {1, 1000};
    TensorShape output_shape = tensor_shape_create(output_dims, 2);
    Tensor output_tensor = tensor_create("output", TENSOR_TYPE_FLOAT32, &output_shape, TENSOR_FORMAT_NC);
    
    // 分配输出内存
    output_tensor.data = malloc(output_tensor.size);
    if (!output_tensor.data) {
        printf("❌ 输出内存分配失败\n");
        tensor_free(&input_tensor);
        model_manager_unload(manager, model);
        model_manager_destroy(manager);
        return 1;
    }
    
    printf("✅ 输出数据准备完成\n");
    printf("   输出形状: [%d, %d]\n", 
           output_tensor.shape.dims[0], output_tensor.shape.dims[1]);
    
    // 5. 执行推理
    printf("\n5. 执行推理...\n");
    
    int ret = model_infer_simple(model, &input_tensor, &output_tensor);
    if (ret != 0) {
        printf("❌ 推理失败，错误码: %d\n", ret);
        tensor_free(&input_tensor);
        tensor_free(&output_tensor);
        model_manager_unload(manager, model);
        model_manager_destroy(manager);
        return 1;
    }
    
    printf("✅ 推理完成\n");
    
    // 6. 处理输出结果
    printf("\n6. 处理输出结果...\n");
    
    float* output_data = (float*)output_tensor.data;
    uint32_t output_count = tensor_get_element_count(&output_tensor);
    
    // 找到最高分数的类别
    float max_score = output_data[0];
    uint32_t max_index = 0;
    
    for (uint32_t i = 1; i < output_count; i++) {
        if (output_data[i] > max_score) {
            max_score = output_data[i];
            max_index = i;
        }
    }
    
    printf("✅ 推理结果:\n");
    printf("   预测类别: %d\n", max_index);
    printf("   置信度: %.4f\n", max_score);
    
    // 显示前5个结果
    printf("\n   前5个预测结果:\n");
    for (int i = 0; i < 5 && i < output_count; i++) {
        printf("   类别 %d: %.4f\n", i, output_data[i]);
    }
    
    // 7. 获取模型信息
    printf("\n7. 模型信息:\n");
    ModelInfo info;
    ret = model_manager_get_info(manager, "test_model", &info);
    if (ret == 0) {
        printf("   模型ID: %s\n", info.model_id);
        printf("   版本: %s\n", info.version);
        printf("   状态: %s\n", info.status == MODEL_STATUS_LOADED ? "已加载" : "未加载");
        printf("   推理次数: %lu\n", info.inference_count);
        printf("   平均延迟: %.2f ms\n", info.avg_latency);
        
        // 释放分配的字符串
        free(info.model_id);
        free(info.version);
    }
    
    // 8. 清理资源
    printf("\n8. 清理资源...\n");
    
    tensor_free(&input_tensor);
    tensor_free(&output_tensor);
    
    ret = model_manager_unload(manager, model);
    if (ret != 0) {
        printf("❌ 模型卸载失败\n");
    } else {
        printf("✅ 模型卸载成功\n");
    }
    
    model_manager_destroy(manager);
    printf("✅ 资源清理完成\n");
    
    printf("\n=== 推理完成 ===\n");
    
    return 0;
} 