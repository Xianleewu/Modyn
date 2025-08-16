#include "core/unified_pipeline.h"
#include "core/tensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 统一推理流水线使用示例
 * 
 * 展示如何使用统一的tensor输入输出接口构建复杂的推理流水线
 */

// ================================
// 自定义处理函数示例
// ================================

/**
 * @brief 图像调整大小处理函数
 * 输入: "image" tensor [B, C, H, W]
 * 输出: "resized_image" tensor [B, C, 224, 224]
 */
int resize_image_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    printf("  [处理单元] 执行图像调整大小...\n");
    
    tensor_t* image = tensor_map_get(inputs, "image");
    if (!image) {
        printf("    错误：未找到输入图像\n");
        return -1;
    }
    
    printf("    输入图像形状: [%u, %u, %u, %u]\n", 
           image->shape.dims[0], image->shape.dims[1], 
           image->shape.dims[2], image->shape.dims[3]);
    
    // 创建调整后的图像tensor
    uint32_t resized_dims[] = {image->shape.dims[0], image->shape.dims[1], 224, 224};
    tensor_shape_t resized_shape = tensor_shape_create(resized_dims, 4);
    
    tensor_t* resized_image = malloc(sizeof(tensor_t));
    *resized_image = tensor_create("resized_image", TENSOR_TYPE_FLOAT32, &resized_shape, TENSOR_FORMAT_NCHW);
    
    resized_image->data = malloc(resized_image->size);
    resized_image->owns_data = true;
    
    if (!resized_image->data) {
        tensor_free(resized_image);
        free(resized_image);
        return -1;
    }
    
    // 简化实现：直接复制部分数据模拟调整大小
    float* src_data = (float*)image->data;
    float* dst_data = (float*)resized_image->data;
    
    size_t copy_size = resized_image->size < image->size ? resized_image->size : image->size;
    if (src_data && dst_data) {
        memcpy(dst_data, src_data, copy_size);
    }
    
    // 添加到输出映射表
    tensor_map_set((tensor_map_t*)outputs, "resized_image", resized_image);
    
    printf("    输出图像形状: [%u, %u, %u, %u]\n", 
           resized_image->shape.dims[0], resized_image->shape.dims[1], 
           resized_image->shape.dims[2], resized_image->shape.dims[3]);
    
    return 0;
}

/**
 * @brief 图像归一化处理函数
 * 输入: "resized_image" tensor
 * 输出: "normalized_image" tensor
 */
int normalize_image_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    printf("  [处理单元] 执行图像归一化...\n");
    
    tensor_t* image = tensor_map_get(inputs, "resized_image");
    if (!image) {
        printf("    错误：未找到调整后的图像\n");
        return -1;
    }
    
    // 创建归一化后的图像tensor
    tensor_t* normalized_image = malloc(sizeof(tensor_t));
    *normalized_image = tensor_copy(image);
    normalized_image->name = strdup("normalized_image");
    
    // 简化实现：假设数据已经归一化
    float* data = (float*)normalized_image->data;
    uint32_t element_count = tensor_get_element_count(normalized_image);
    
    // 应用归一化 (0-255) -> (0-1)
    for (uint32_t i = 0; i < element_count; i++) {
        data[i] = data[i] / 255.0f;
    }
    
    tensor_map_set((tensor_map_t*)outputs, "normalized_image", normalized_image);
    
    printf("    图像归一化完成\n");
    return 0;
}

/**
 * @brief 分类结果后处理函数
 * 输入: "model_output" tensor [1, num_classes]
 * 输出: "class_id" tensor [1], "confidence" tensor [1]
 */
int postprocess_classification_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    printf("  [处理单元] 执行分类后处理...\n");
    
    tensor_t* logits = tensor_map_get(inputs, "model_output");
    if (!logits) {
        printf("    错误：未找到模型输出\n");
        return -1;
    }
    
    float* logit_data = (float*)logits->data;
    uint32_t num_classes = logits->shape.dims[1];
    
    // 找到最大值和对应的类别
    float max_score = logit_data[0];
    int32_t max_class = 0;
    
    for (uint32_t i = 1; i < num_classes; i++) {
        if (logit_data[i] > max_score) {
            max_score = logit_data[i];
            max_class = i;
        }
    }
    
    // 创建类别ID tensor
    uint32_t class_dims[] = {1};
    tensor_shape_t class_shape = tensor_shape_create(class_dims, 1);
    tensor_t* class_tensor = malloc(sizeof(tensor_t));
    *class_tensor = tensor_create("class_id", TENSOR_TYPE_INT32, &class_shape, TENSOR_FORMAT_N);
    
    class_tensor->data = malloc(class_tensor->size);
    class_tensor->owns_data = true;
    *((int32_t*)class_tensor->data) = max_class;
    
    // 创建置信度tensor
    tensor_t* confidence_tensor = malloc(sizeof(tensor_t));
    *confidence_tensor = tensor_create("confidence", TENSOR_TYPE_FLOAT32, &class_shape, TENSOR_FORMAT_N);
    
    confidence_tensor->data = malloc(confidence_tensor->size);
    confidence_tensor->owns_data = true;
    *((float*)confidence_tensor->data) = max_score;
    
    tensor_map_set((tensor_map_t*)outputs, "class_id", class_tensor);
    tensor_map_set((tensor_map_t*)outputs, "confidence", confidence_tensor);
    
    printf("    预测类别: %d, 置信度: %.3f\n", max_class, max_score);
    return 0;
}

// ================================
// 主函数：图像分类流水线示例
// ================================

int main(int argc, char* argv[]) {
    printf("=== 统一推理流水线示例：图像分类 ===\n\n");
    
    // 1. 创建统一流水线
    printf("1. 创建统一流水线...\n");
    unified_pipeline_t* pipeline = unified_pipeline_create("image_classification_pipeline");
    if (!pipeline) {
        printf("创建流水线失败\n");
        return -1;
    }
    
    // 启用调试模式
    unified_pipeline_set_debug_mode(pipeline, true);
    
    // 2. 添加处理单元
    printf("\n2. 添加处理单元...\n");
    
    // 2.1 图像调整大小单元
    const char* resize_inputs[] = {"image"};
    const char* resize_outputs[] = {"resized_image"};
    processing_unit_t* resize_unit = create_function_unit(
        "image_resize", 
        resize_image_func, 
        NULL,  // 无需额外上下文
        resize_inputs, 1,
        resize_outputs, 1
    );
    
    if (!resize_unit || unified_pipeline_add_unit(pipeline, resize_unit) != 0) {
        printf("添加图像调整单元失败\n");
        unified_pipeline_destroy(pipeline);
        return -1;
    }
    printf("  ✅ 添加图像调整大小单元\n");
    
    // 2.2 图像归一化单元
    const char* norm_inputs[] = {"resized_image"};
    const char* norm_outputs[] = {"normalized_image"};
    processing_unit_t* normalize_unit = create_function_unit(
        "image_normalize",
        normalize_image_func,
        NULL,
        norm_inputs, 1,
        norm_outputs, 1
    );
    
    if (!normalize_unit || unified_pipeline_add_unit(pipeline, normalize_unit) != 0) {
        printf("添加图像归一化单元失败\n");
        unified_pipeline_destroy(pipeline);
        return -1;
    }
    printf("  ✅ 添加图像归一化单元\n");
    
    // 2.3 模型推理单元（简化实现）
    const char* model_inputs[] = {"normalized_image"};
    const char* model_outputs[] = {"model_output"};
    processing_unit_t* model_unit = create_model_unit(
        "classification_model",
        "resnet50.onnx",  // 模型路径
        model_inputs, 1,
        model_outputs, 1
    );
    
    if (!model_unit || unified_pipeline_add_unit(pipeline, model_unit) != 0) {
        printf("添加模型推理单元失败\n");
        unified_pipeline_destroy(pipeline);
        return -1;
    }
    printf("  ✅ 添加模型推理单元\n");
    
    // 2.4 分类后处理单元
    const char* post_inputs[] = {"model_output"};
    const char* post_outputs[] = {"class_id", "confidence"};
    processing_unit_t* postprocess_unit = create_function_unit(
        "classification_postprocess",
        postprocess_classification_func,
        NULL,
        post_inputs, 1,
        post_outputs, 2
    );
    
    if (!postprocess_unit || unified_pipeline_add_unit(pipeline, postprocess_unit) != 0) {
        printf("添加后处理单元失败\n");
        unified_pipeline_destroy(pipeline);
        return -1;
    }
    printf("  ✅ 添加分类后处理单元\n");
    
    // 3. 准备输入数据
    printf("\n3. 准备输入数据...\n");
    
    // 创建输入图像tensor (1, 3, 256, 256)
    uint32_t input_dims[] = {1, 3, 256, 256};
    tensor_shape_t input_shape = tensor_shape_create(input_dims, 4);
    tensor_t* input_image = malloc(sizeof(tensor_t));
    *input_image = tensor_create("image", TENSOR_TYPE_FLOAT32, &input_shape, TENSOR_FORMAT_NCHW);
    
    input_image->data = malloc(input_image->size);
    input_image->owns_data = true;
    
    if (!input_image->data) {
        printf("分配输入数据内存失败\n");
        tensor_free(input_image);
        free(input_image);
        unified_pipeline_destroy(pipeline);
        return -1;
    }
    
    // 填充虚拟图像数据
    float* image_data = (float*)input_image->data;
    uint32_t element_count = tensor_get_element_count(input_image);
    
    for (uint32_t i = 0; i < element_count; i++) {
        image_data[i] = (float)(rand() % 256); // 模拟0-255的像素值
    }
    
    printf("  输入图像形状: [%u, %u, %u, %u]\n", 
           input_image->shape.dims[0], input_image->shape.dims[1], 
           input_image->shape.dims[2], input_image->shape.dims[3]);
    
    // 4. 创建输入输出映射表
    printf("\n4. 创建tensor映射表...\n");
    
    tensor_map_t* inputs = tensor_map_create(8);
    tensor_map_t* outputs = tensor_map_create(8);
    
    if (!inputs || !outputs) {
        printf("创建tensor映射表失败\n");
        tensor_free(input_image);
        free(input_image);
        unified_pipeline_destroy(pipeline);
        return -1;
    }
    
    // 添加输入tensor到映射表
    tensor_map_set(inputs, "image", input_image);
    printf("  ✅ 输入映射表准备完成\n");
    
    // 5. 执行流水线
    printf("\n5. 执行推理流水线...\n");
    
    int result = unified_pipeline_execute(pipeline, inputs, outputs);
    
    if (result == 0) {
        printf("\n✅ 流水线执行成功！\n");
        
        // 6. 获取结果
        printf("\n6. 获取推理结果...\n");
        
        tensor_t* class_id = tensor_map_get(outputs, "class_id");
        tensor_t* confidence = tensor_map_get(outputs, "confidence");
        
        if (class_id && confidence) {
            int32_t predicted_class = *((int32_t*)class_id->data);
            float conf_score = *((float*)confidence->data);
            
            printf("  🎯 预测结果:\n");
            printf("    类别ID: %d\n", predicted_class);
            printf("    置信度: %.3f\n", conf_score);
        } else {
            printf("  ❌ 未找到预测结果\n");
        }
        
        // 显示所有输出tensor
        printf("\n  📊 所有输出tensor:\n");
        for (uint32_t i = 0; i < tensor_map_size(outputs); i++) {
            printf("    - %s: ", outputs->keys[i]);
            tensor_print_info(outputs->tensors[i]);
        }
        
    } else {
        printf("\n❌ 流水线执行失败，错误代码: %d\n", result);
    }
    
    // 7. 清理资源
    printf("\n7. 清理资源...\n");
    
    // 释放tensor资源（注意：输出中的tensor可能需要手动释放）
    for (uint32_t i = 0; i < tensor_map_size(outputs); i++) {
        tensor_t* tensor = outputs->tensors[i];
        if (tensor && tensor != input_image) { // 不要重复释放输入tensor
            tensor_free(tensor);
            free(tensor);
        }
    }
    
    tensor_free(input_image);
    free(input_image);
    
    tensor_map_destroy(inputs);
    tensor_map_destroy(outputs);
    unified_pipeline_destroy(pipeline);
    
    printf("  ✅ 资源清理完成\n");
    
    printf("\n=== 示例执行完成 ===\n");
    return 0;
}

// ================================
// 编译和运行说明
// ================================
/*
编译命令:
gcc -o unified_pipeline_example examples/unified_pipeline_example.c \
    core/unified_pipeline.c core/tensor.c \
    -I. -lm -lpthread

运行命令:
./unified_pipeline_example

预期输出:
=== 统一推理流水线示例：图像分类 ===

1. 创建统一流水线...

2. 添加处理单元...
  ✅ 添加图像调整大小单元
  ✅ 添加图像归一化单元
  ✅ 添加模型推理单元
  ✅ 添加分类后处理单元

3. 准备输入数据...
  输入图像形状: [1, 3, 256, 256]

4. 创建tensor映射表...
  ✅ 输入映射表准备完成

5. 执行推理流水线...
执行处理单元: image_resize
  [处理单元] 执行图像调整大小...
    输入图像形状: [1, 3, 256, 256]
    输出图像形状: [1, 3, 224, 224]
处理单元 'image_resize' 执行完成: 结果=0, 耗时=X.XXms

执行处理单元: image_normalize
  [处理单元] 执行图像归一化...
    图像归一化完成
处理单元 'image_normalize' 执行完成: 结果=0, 耗时=X.XXms

执行处理单元: classification_model
处理单元 'classification_model' 执行完成: 结果=0, 耗时=X.XXms

执行处理单元: classification_postprocess
  [处理单元] 执行分类后处理...
    预测类别: 123, 置信度: 0.856
处理单元 'classification_postprocess' 执行完成: 结果=0, 耗时=X.XXms

✅ 流水线执行成功！

6. 获取推理结果...
  🎯 预测结果:
    类别ID: 123
    置信度: 0.856

  📊 所有输出tensor:
    - class_id: Tensor: class_id ...
    - confidence: Tensor: confidence ...
    - model_output: Tensor: model_output ...
    - normalized_image: Tensor: normalized_image ...
    - resized_image: Tensor: resized_image ...

7. 清理资源...
  ✅ 资源清理完成

=== 示例执行完成 ===

关键特性展示:
1. 统一的tensor输入输出接口 - 所有处理单元都使用相同的接口
2. 灵活的流水线组合 - 可以任意组合不同类型的处理单元
3. 透明的数据流 - tensor在处理单元间自动传递
4. 类型无关的处理 - 不区分预处理、模型推理、后处理等
5. 易于扩展 - 添加新的处理单元非常简单
*/