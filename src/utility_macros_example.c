/**
 * @file utility_macros_example.c
 * @brief 工具宏使用示例
 */

#define MODYN_DEBUG
#define MODYN_ENABLE_LOGGING
#define MODYN_ENABLE_PROFILING

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "modyn.h"

// ======================= 编译器属性示例 ======================= //

MODYN_INLINE int fast_add(int a, int b) {
    return a + b;
}

MODYN_DEPRECATED int old_function(void) {
    return 42;
}

typedef struct MODYN_PACKED {
    uint8_t flag;
    uint32_t value;
    uint8_t data;
} modyn_packed_struct_t;

typedef struct {
    int id MODYN_ALIGN(16);
    float values[4] MODYN_ALIGN(32);
} modyn_aligned_struct_t;

// ======================= 错误处理示例 ======================= //

static modyn_status_e validate_tensor_shape(const modyn_tensor_shape_t *shape) {
    MODYN_CHECK_NULL(shape);
    
    MODYN_VALIDATE_RANGE(shape->num_dims, 1, MODYN_TENSOR_SHAPE_MAX_DIMS);
    
    for (size_t i = 0; i < shape->num_dims; i++) {
        MODYN_VALIDATE_RANGE(shape->dims[i], 1, 65536);
    }
    
    return MODYN_SUCCESS;
}

static modyn_status_e create_tensor_buffer(const modyn_tensor_shape_t *shape, 
                                       modyn_data_type_e dtype,
                                       modyn_tensor_data_t *tensor) {
    MODYN_CHECK_NULL(shape);
    MODYN_CHECK_NULL(tensor);
    
    // 验证形状
    MODYN_RETURN_IF_ERROR(validate_tensor_shape(shape));
    
    // 计算所需内存大小
    size_t element_count = MODYN_SHAPE_ELEMENT_COUNT(shape);
    size_t buffer_size = MODYN_TENSOR_SIZE_BYTES(shape, dtype);
    
    MODYN_LOG_INFO("创建张量缓冲区: %zu 个元素, %zu 字节", element_count, buffer_size);
    
    // 分配内存
    tensor->data = MODYN_MALLOC(buffer_size);
    tensor->shape = *shape;
    tensor->dtype = dtype;
    tensor->size = buffer_size;
    tensor->mem_type = MODYN_MEMORY_INTERNAL;
    
    return MODYN_SUCCESS;
}

// ======================= 性能测量示例 ======================= //

static void example_performance_measurement(void) {
    MODYN_LOG_INFO("=== 性能测量示例 ===");
    
    // 单独计时
    MODYN_TIMER_START(matrix_multiply);
    
    // 模拟矩阵乘法
    const int size = 1000;
    float *matrix_a = malloc(size * size * sizeof(float));
    float *matrix_b = malloc(size * size * sizeof(float));
    float *result = malloc(size * size * sizeof(float));
    
    // 初始化矩阵
    for (int i = 0; i < size * size; i++) {
        matrix_a[i] = (float)i / 1000.0f;
        matrix_b[i] = (float)(i * 2) / 1000.0f;
    }
    
    // 矩阵乘法
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            float sum = 0.0f;
            for (int k = 0; k < size; k++) {
                sum += matrix_a[i * size + k] * matrix_b[k * size + j];
            }
            result[i * size + j] = sum;
        }
    }
    
    MODYN_TIMER_END(matrix_multiply);
    
    MODYN_LOG_INFO("矩阵乘法 (%dx%d) 耗时: %.3f ms", 
                size, size, MODYN_TIMER_ELAPSED_MS(matrix_multiply));
    
    // 使用作用域计时宏
    MODYN_PROFILE_SCOPE(memory_cleanup, {
        free(matrix_a);
        free(matrix_b);
        free(result);
    });
}

// ======================= 内存和位操作示例 ======================= //

static void example_memory_and_bit_operations(void) {
    MODYN_LOG_INFO("=== 内存和位操作示例 ===");
    
    // 内存对齐示例
    size_t size = 1000;
    size_t aligned_size = MODYN_ALIGN_UP(size, 64);  // 64字节对齐
    
    MODYN_LOG_INFO("原始大小: %zu, 对齐后: %zu", size, aligned_size);
    MODYN_LOG_INFO("是否64字节对齐: %s", MODYN_IS_ALIGNED(aligned_size, 64) ? "是" : "否");
    
    // 位操作示例
    uint32_t flags = 0;
    
    MODYN_SET_BIT(flags, 3);       // 设置第3位
    MODYN_SET_BIT(flags, 7);       // 设置第7位
    
    MODYN_LOG_INFO("标志位: 0x%08X", flags);
    MODYN_LOG_INFO("第3位是否设置: %s", MODYN_TEST_BIT(flags, 3) ? "是" : "否");
    MODYN_LOG_INFO("第5位是否设置: %s", MODYN_TEST_BIT(flags, 5) ? "是" : "否");
    
    MODYN_TOGGLE_BIT(flags, 3);    // 切换第3位
    MODYN_LOG_INFO("切换第3位后: 0x%08X", flags);
    
    // 设备能力检查示例
    uint32_t device_caps = MODYN_DEVICE_CAP_FLOAT16 | MODYN_DEVICE_CAP_INT8 | MODYN_DEVICE_CAP_CNN;
    uint32_t required_caps = MODYN_DEVICE_CAP_FLOAT16 | MODYN_DEVICE_CAP_CNN;
    
    if (MODYN_DEVICE_SUPPORTS(device_caps, MODYN_DEVICE_CAP_FLOAT16)) {
        MODYN_LOG_INFO("设备支持FP16");
    }
    
    if (MODYN_DEVICE_REQUIRES_ALL(device_caps, required_caps)) {
        MODYN_LOG_INFO("设备满足所有必需能力");
    }
}

// ======================= 张量操作示例 ======================= //

static void example_tensor_operations(void) {
    MODYN_LOG_INFO("=== 张量操作示例 ===");
    
    // 创建4D张量 (NCHW格式)
    modyn_tensor_shape_t shape = {
        .num_dims = 4,
        .dims = {1, 3, 224, 224}  // 批次=1, 通道=3, 高=224, 宽=224
    };
    
    if (MODYN_TENSOR_IS_VALID_SHAPE(&shape)) {
        size_t element_count = MODYN_SHAPE_ELEMENT_COUNT(&shape);
        size_t tensor_bytes = MODYN_TENSOR_SIZE_BYTES(&shape, MODYN_DATA_TYPE_FLOAT32);
        
        MODYN_LOG_INFO("张量形状: [%zu, %zu, %zu, %zu]", 
                    shape.dims[0], shape.dims[1], shape.dims[2], shape.dims[3]);
        MODYN_LOG_INFO("元素总数: %zu", element_count);
        MODYN_LOG_INFO("内存大小: %zu 字节 (%.2f MB)", 
                    tensor_bytes, (double)tensor_bytes / MODYN_MB(1));
        
        // 计算张量索引
        size_t n = 0, c = 1, h = 100, w = 150;
        size_t offset = MODYN_TENSOR_OFFSET_4D(n, c, h, w, 
                                           shape.dims[0], shape.dims[1], 
                                           shape.dims[2], shape.dims[3]);
        
        MODYN_LOG_INFO("位置 [%zu,%zu,%zu,%zu] 的偏移量: %zu", n, c, h, w, offset);
        
        // 创建张量缓冲区
        modyn_tensor_data_t tensor;
        modyn_status_e status = create_tensor_buffer(&shape, MODYN_DATA_TYPE_FLOAT32, &tensor);
        
        if (status == MODYN_SUCCESS) {
            MODYN_LOG_INFO("张量缓冲区创建成功");
            
            // 填充数据
            float *data = (float*)tensor.data;
            for (size_t i = 0; i < element_count; i++) {
                data[i] = (float)i / element_count;
            }
            
            // 访问特定位置的数据
            float value = data[offset];
            MODYN_LOG_INFO("位置 [%zu,%zu,%zu,%zu] 的值: %.6f", n, c, h, w, value);
            
            MODYN_FREE(tensor.data);
        } else {
            MODYN_LOG_ERROR("张量缓冲区创建失败: %d", status);
        }
    }
}

// ======================= 字符串和工具示例 ======================= //

static void example_string_utilities(void) {
    MODYN_LOG_INFO("=== 字符串工具示例 ===");
    
    const char *model_path = "/path/to/model.onnx";
    const char *null_str = NULL;
    
    MODYN_LOG_INFO("模型路径: %s", MODYN_SAFE_STRING(model_path));
    MODYN_LOG_INFO("空字符串: '%s'", MODYN_SAFE_STRING(null_str));
    
    if (MODYN_STRING_STARTS_WITH(model_path, "/path/to/")) {
        MODYN_LOG_INFO("模型路径以'/path/to/'开头");
    }
    
    const char *ext1 = ".onnx";
    const char *ext2 = ".rknn";
    
    if (MODYN_STRING_EQUALS(ext1, ext2)) {
        MODYN_LOG_INFO("扩展名相同");
    } else {
        MODYN_LOG_INFO("扩展名不同: %s vs %s", ext1, ext2);
    }
    
    // 版本信息
    MODYN_LOG_INFO("框架版本: %s", MODYN_VERSION_STRING);
    MODYN_LOG_INFO("版本号: 0x%08X", MODYN_VERSION_NUMBER);
    MODYN_LOG_INFO("构建类型: %s", MODYN_BUILD_TYPE);
    MODYN_LOG_INFO("编译器: %s", MODYN_COMPILER);
}

// ======================= 容器操作示例 ======================= //

typedef struct {
    int id;
    char name[32];
    modyn_tensor_data_t tensor;
} modyn_model_instance_t;

static void example_container_operations(void) {
    MODYN_LOG_INFO("=== 容器操作示例 ===");
    
    // 数组大小
    int priorities[] = {1, 5, 3, 9, 2, 7};
    size_t array_size = MODYN_ARRAY_SIZE(priorities);
    
    MODYN_LOG_INFO("优先级数组大小: %zu", array_size);
    
    // 找到最大最小值
    int min_priority = priorities[0];
    int max_priority = priorities[0];
    
    for (size_t i = 1; i < array_size; i++) {
        min_priority = MODYN_MIN(min_priority, priorities[i]);
        max_priority = MODYN_MAX(max_priority, priorities[i]);
    }
    
    MODYN_LOG_INFO("优先级范围: %d - %d", min_priority, max_priority);
    
    // 容器宏示例
    modyn_model_instance_t instance = { .id = 100 };
    modyn_tensor_data_t *tensor_ptr = &instance.tensor;
    
    // 从成员指针获取容器指针
    modyn_model_instance_t *container = MODYN_CONTAINER_OF(tensor_ptr, modyn_model_instance_t, tensor);
    MODYN_LOG_INFO("容器ID: %d", container->id);
    
    // 内存大小宏
    MODYN_LOG_INFO("缓存大小: %zu KB", MODYN_KB(256) / 1024);
    MODYN_LOG_INFO("模型大小: %zu MB", MODYN_MB(100) / (1024 * 1024));
}

// ======================= 调试和错误处理示例 ======================= //

static modyn_status_e example_error_handling(void) {
    MODYN_LOG_INFO("=== 错误处理示例 ===");
    
    modyn_status_e status = MODYN_SUCCESS;
    modyn_tensor_shape_t *shape = NULL;
    void *buffer = NULL;
    
    // 使用goto错误处理
    MODYN_GOTO_IF_ERROR(validate_tensor_shape(shape), cleanup);
    
    shape = malloc(sizeof(modyn_tensor_shape_t));
    MODYN_GOTO_IF_ERROR(shape ? MODYN_SUCCESS : MODYN_ERROR_MEMORY_ALLOCATION, cleanup);
    
    // 设置有效形状
    shape->num_dims = 3;
    shape->dims[0] = 100;
    shape->dims[1] = 200;
    shape->dims[2] = 3;
    
    MODYN_GOTO_IF_ERROR(validate_tensor_shape(shape), cleanup);
    
    size_t buffer_size = MODYN_TENSOR_SIZE_BYTES(shape, MODYN_DATA_TYPE_UINT8);
    buffer = malloc(buffer_size);
    MODYN_GOTO_IF_ERROR(buffer ? MODYN_SUCCESS : MODYN_ERROR_MEMORY_ALLOCATION, cleanup);
    
    MODYN_LOG_INFO("成功分配 %zu 字节缓冲区", buffer_size);
    status = MODYN_SUCCESS;
    
cleanup:
    MODYN_FREE(shape);
    MODYN_FREE(buffer);
    
    if (status != MODYN_SUCCESS) {
        MODYN_LOG_ERROR("操作失败，状态码: %d", status);
    }
    
    return status;
}

// ======================= 主函数 ======================= //

int main(void) {
    MODYN_LOG_INFO("=== 工具宏使用示例 ===");
    
    // 调试输出
    MODYN_DEBUG_PRINT("这是调试信息");
    
    // 各种示例
    example_performance_measurement();
    example_memory_and_bit_operations();
    example_tensor_operations();
    example_string_utilities();
    example_container_operations();
    example_error_handling();
    
    // 使用可能性提示
    int random_value = rand() % 100;
    
    if (MODYN_LIKELY(random_value < 90)) {
        MODYN_LOG_INFO("高概率事件发生 (值: %d)", random_value);
    } else if (MODYN_UNLIKELY(random_value >= 95)) {
        MODYN_LOG_INFO("低概率事件发生 (值: %d)", random_value);
    }
    
    // 测试已弃用函数
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int old_result = old_function();
    #pragma GCC diagnostic pop
    
    MODYN_LOG_INFO("旧函数结果: %d", old_result);
    
    MODYN_LOG_INFO("所有示例执行完成");
    return 0;
}