#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "core/tensor.h"
#include "utils/logger.h"

/**
 * @brief 张量单元测试
 */

// 测试张量创建和释放
void test_tensor_create_free(void) {
    printf("测试张量创建和释放...\n");
    
    // 创建张量形状
    uint32_t dims[] = {1, 3, 224, 224};
    TensorShape shape = tensor_shape_create(dims, 4);
    
    // 创建张量
    Tensor tensor = tensor_create("test_tensor", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NCHW);
    
    // 验证基本属性
    assert(tensor.name != NULL);
    assert(strcmp(tensor.name, "test_tensor") == 0);
    assert(tensor.dtype == TENSOR_TYPE_FLOAT32);
    assert(tensor.format == TENSOR_FORMAT_NCHW);
    assert(tensor.shape.ndim == 4);
    assert(tensor.shape.dims[0] == 1);
    assert(tensor.shape.dims[1] == 3);
    assert(tensor.shape.dims[2] == 224);
    assert(tensor.shape.dims[3] == 224);
    assert(tensor.memory_type == TENSOR_MEMORY_CPU);
    assert(tensor.size == 1 * 3 * 224 * 224 * sizeof(float));
    assert(tensor.data == NULL);
    assert(tensor.owns_data == false);
    assert(tensor.ref_count == 1);
    
    // 释放张量
    tensor_free(&tensor);
    
    printf("✅ 张量创建和释放测试通过\n");
}

// 测试张量数据操作
void test_tensor_data_operations(void) {
    printf("测试张量数据操作...\n");
    
    uint32_t dims[] = {2, 3};
    TensorShape shape = tensor_shape_create(dims, 2);
    Tensor tensor = tensor_create("data_tensor", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NC);
    
    // 分配数据
    tensor.data = malloc(tensor.size);
    tensor.owns_data = true;
    assert(tensor.data != NULL);
    
    // 填充数据
    float* data = (float*)tensor.data;
    for (uint32_t i = 0; i < tensor_get_element_count(&tensor); i++) {
        data[i] = (float)i * 0.1f;
    }
    
    // 验证数据
    for (uint32_t i = 0; i < tensor_get_element_count(&tensor); i++) {
        assert(fabs(data[i] - i * 0.1f) < 1e-6);
    }
    
    // 测试元素数量计算
    assert(tensor_get_element_count(&tensor) == 6);
    
    tensor_free(&tensor);
    
    printf("✅ 张量数据操作测试通过\n");
}

// 测试张量复制
void test_tensor_copy(void) {
    printf("测试张量复制...\n");
    
    uint32_t dims[] = {2, 2};
    TensorShape shape = tensor_shape_create(dims, 2);
    Tensor original = tensor_create("original", TENSOR_TYPE_INT32, &shape, TENSOR_FORMAT_NC);
    
    // 分配并填充数据
    original.data = malloc(original.size);
    original.owns_data = true;
    int32_t* data = (int32_t*)original.data;
    for (int i = 0; i < 4; i++) {
        data[i] = i + 10;
    }
    
    // 复制张量
    Tensor copy = tensor_copy(&original);
    
    // 验证复制结果
    assert(copy.name != NULL);
    assert(strcmp(copy.name, "original") == 0);
    assert(copy.dtype == TENSOR_TYPE_INT32);
    assert(copy.format == TENSOR_FORMAT_NC);
    assert(copy.shape.ndim == 2);
    assert(copy.shape.dims[0] == 2);
    assert(copy.shape.dims[1] == 2);
    assert(copy.size == original.size);
    assert(copy.data != original.data);  // 不同的内存地址
    assert(copy.owns_data == true);
    
    // 验证数据内容
    int32_t* copy_data = (int32_t*)copy.data;
    for (int i = 0; i < 4; i++) {
        assert(copy_data[i] == data[i]);
    }
    
    // 修改原始数据，复制的数据应该不变
    data[0] = 999;
    assert(copy_data[0] == 10);
    
    tensor_free(&original);
    tensor_free(&copy);
    
    printf("✅ 张量复制测试通过\n");
}

// 测试张量形状操作
void test_tensor_shape_operations(void) {
    printf("测试张量形状操作...\n");
    
    // 测试形状创建
    uint32_t dims1[] = {2, 3, 4};
    TensorShape shape1 = tensor_shape_create(dims1, 3);
    assert(shape1.ndim == 3);
    assert(shape1.dims[0] == 2);
    assert(shape1.dims[1] == 3);
    assert(shape1.dims[2] == 4);
    
    // 测试形状比较
    uint32_t dims2[] = {2, 3, 4};
    TensorShape shape2 = tensor_shape_create(dims2, 3);
    assert(tensor_shape_equal(&shape1, &shape2) == true);
    
    uint32_t dims3[] = {2, 3, 5};
    TensorShape shape3 = tensor_shape_create(dims3, 3);
    assert(tensor_shape_equal(&shape1, &shape3) == false);
    
    uint32_t dims4[] = {2, 3};
    TensorShape shape4 = tensor_shape_create(dims4, 2);
    assert(tensor_shape_equal(&shape1, &shape4) == false);
    
    // 测试重塑
    Tensor tensor = tensor_create("reshape_test", TENSOR_TYPE_FLOAT32, &shape1, TENSOR_FORMAT_NCHW);
    
    uint32_t new_dims[] = {6, 4};
    TensorShape new_shape = tensor_shape_create(new_dims, 2);
    assert(tensor_reshape(&tensor, &new_shape) == 0);
    
    assert(tensor.shape.ndim == 2);
    assert(tensor.shape.dims[0] == 6);
    assert(tensor.shape.dims[1] == 4);
    
    // 测试无效重塑（元素数量不匹配）
    uint32_t invalid_dims[] = {3, 3};
    TensorShape invalid_shape = tensor_shape_create(invalid_dims, 2);
    assert(tensor_reshape(&tensor, &invalid_shape) != 0);
    
    tensor_free(&tensor);
    
    printf("✅ 张量形状操作测试通过\n");
}

// 测试数据类型大小
void test_tensor_dtype_size(void) {
    printf("测试数据类型大小...\n");
    
    assert(tensor_get_dtype_size(TENSOR_TYPE_FLOAT32) == sizeof(float));
    assert(tensor_get_dtype_size(TENSOR_TYPE_FLOAT16) == 2);
    assert(tensor_get_dtype_size(TENSOR_TYPE_INT32) == sizeof(int32_t));
    assert(tensor_get_dtype_size(TENSOR_TYPE_INT16) == sizeof(int16_t));
    assert(tensor_get_dtype_size(TENSOR_TYPE_INT8) == sizeof(int8_t));
    assert(tensor_get_dtype_size(TENSOR_TYPE_UINT8) == sizeof(uint8_t));
    assert(tensor_get_dtype_size(TENSOR_TYPE_BOOL) == sizeof(bool));
    assert(tensor_get_dtype_size(TENSOR_TYPE_UNKNOWN) == 0);
    
    printf("✅ 数据类型大小测试通过\n");
}

// 测试从现有数据创建张量
void test_tensor_from_data(void) {
    printf("测试从现有数据创建张量...\n");
    
    // 准备数据
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    size_t data_size = sizeof(data);
    
    uint32_t dims[] = {2, 3};
    TensorShape shape = tensor_shape_create(dims, 2);
    
    // 测试不拥有数据的情况
    Tensor tensor1 = tensor_from_data("external_data", TENSOR_TYPE_FLOAT32, &shape, 
                                     TENSOR_FORMAT_NC, data, data_size, false);
    
    assert(tensor1.data == data);
    assert(tensor1.size == data_size);
    assert(tensor1.owns_data == false);
    
    // 验证数据可访问
    float* tensor_data = (float*)tensor1.data;
    assert(tensor_data[0] == 1.0f);
    assert(tensor_data[5] == 6.0f);
    
    // 释放时不应该释放数据
    tensor_free(&tensor1);
    assert(data[0] == 1.0f);  // 数据应该仍然有效
    
    // 测试拥有数据的情况
    float* copied_data = malloc(data_size);
    memcpy(copied_data, data, data_size);
    
    Tensor tensor2 = tensor_from_data("owned_data", TENSOR_TYPE_FLOAT32, &shape,
                                     TENSOR_FORMAT_NC, copied_data, data_size, true);
    
    assert(tensor2.data == copied_data);
    assert(tensor2.owns_data == true);
    
    tensor_free(&tensor2);  // 应该释放 copied_data
    
    printf("✅ 从现有数据创建张量测试通过\n");
}

// 测试格式转换
void test_tensor_format_conversion(void) {
    printf("测试张量格式转换...\n");
    
    uint32_t dims[] = {1, 3, 2, 2};
    TensorShape shape = tensor_shape_create(dims, 4);
    Tensor tensor = tensor_create("format_test", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NCHW);
    
    // 分配数据内存并初始化
    size_t size = tensor_get_element_count(&tensor) * tensor_get_dtype_size(tensor.dtype);
    tensor.data = malloc(size);
    tensor.size = size;
    tensor.owns_data = true;
    float* data = (float*)tensor.data;
    for (size_t i = 0; i < size / sizeof(float); i++) {
        data[i] = (float)i;
    }
    printf("初始format=%d, ndim=%u\n", tensor.format, tensor.shape.ndim);
    assert(tensor.format == TENSOR_FORMAT_NCHW);
    int ret = tensor_convert_format(&tensor, TENSOR_FORMAT_NHWC);
    printf("转换返回值: %d, 转换后format=%d\n", ret, tensor.format);
    assert(ret == 0);
    assert(tensor.format == TENSOR_FORMAT_NHWC);
    ret = tensor_convert_format(&tensor, TENSOR_FORMAT_NHWC);
    printf("再次转换返回值: %d, format=%d\n", ret, tensor.format);
    assert(ret == 0);
    tensor_free(&tensor);
    printf("✅ 张量格式转换测试通过\n");
}

// 测试边界条件
void test_tensor_boundary_conditions(void) {
    printf("测试张量边界条件...\n");
    
    // 测试NULL参数
    assert(tensor_get_element_count(NULL) == 0);
    
    Tensor empty_tensor = {0};
    assert(tensor_get_element_count(&empty_tensor) == 0);
    
    // 测试0维张量
    TensorShape zero_shape = {0};
    Tensor zero_tensor = tensor_create("zero", TENSOR_TYPE_FLOAT32, &zero_shape, TENSOR_FORMAT_N);
    assert(tensor_get_element_count(&zero_tensor) == 0);
    tensor_free(&zero_tensor);
    
    // 测试最大维度
    uint32_t max_dims[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    TensorShape max_shape = tensor_shape_create(max_dims, 8);
    assert(max_shape.ndim == 8);
    
    // 测试超过最大维度（应该被截断）
    uint32_t over_dims[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    TensorShape over_shape = tensor_shape_create(over_dims, 10);
    assert(over_shape.ndim == 8);  // 应该被截断到8
    
    printf("✅ 张量边界条件测试通过\n");
}

int main(void) {
    // 初始化日志系统
    logger_init(LOG_LEVEL_INFO, NULL);
    logger_set_console_output(true);
    
    printf("=== 张量单元测试 ===\n");
    
    test_tensor_create_free();
    test_tensor_data_operations();
    test_tensor_copy();
    test_tensor_shape_operations();
    test_tensor_dtype_size();
    test_tensor_from_data();
    test_tensor_format_conversion();
    test_tensor_boundary_conditions();
    
    printf("\n🎉 所有张量测试通过！\n");
    
    logger_cleanup();
    return 0;
} 