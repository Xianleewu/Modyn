#ifndef MODYN_CORE_TENSOR_H
#define MODYN_CORE_TENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 张量最大维度数
 */
#define TENSOR_MAX_DIMS 8

/**
 * @brief 数据类型枚举
 */
typedef enum {
    TENSOR_TYPE_UNKNOWN = 0,
    TENSOR_TYPE_FLOAT32,        /**< 32位浮点数 */
    TENSOR_TYPE_FLOAT64,        /**< 64位浮点数 */
    TENSOR_TYPE_FLOAT16,        /**< 16位浮点数 */
    TENSOR_TYPE_INT32,          /**< 32位整数 */
    TENSOR_TYPE_INT64,          /**< 64位整数 */
    TENSOR_TYPE_INT16,          /**< 16位整数 */
    TENSOR_TYPE_INT8,           /**< 8位整数 */
    TENSOR_TYPE_UINT8,          /**< 8位无符号整数 */
    TENSOR_TYPE_BOOL,           /**< 布尔类型 */
    TENSOR_TYPE_STRING          /**< 字符串类型 */
} tensor_data_type_e;

/**
 * @brief 张量格式枚举
 */
typedef enum {
    TENSOR_FORMAT_NCHW = 0,     /**< Batch, Channel, Height, Width */
    TENSOR_FORMAT_NHWC,         /**< Batch, Height, Width, Channel */
    TENSOR_FORMAT_NC,           /**< Batch, Channel */
    TENSOR_FORMAT_N             /**< Batch */
} tensor_format_e;

/**
 * @brief 张量内存类型
 */
typedef enum {
    TENSOR_MEMORY_CPU = 0,      /**< CPU内存 */
    TENSOR_MEMORY_GPU,          /**< GPU内存 */
    TENSOR_MEMORY_NPU,          /**< NPU内存 */
    TENSOR_MEMORY_SHARED,       /**< 共享内存 */
    TENSOR_MEMORY_EXTERNAL      /**< 外部内存（zero-copy） */
} tensor_memory_type_e;

/**
 * @brief 张量形状结构
 */
typedef struct {
    uint32_t dims[8];           /**< 维度数组 */
    uint32_t ndim;              /**< 维度数量 */
} tensor_shape_t;

/**
 * @brief 张量结构
 */
typedef struct {
    char* name;                 /**< 张量名称 */
    tensor_data_type_e dtype;   /**< 数据类型 */
    tensor_shape_t shape;       /**< 张量形状 */
    tensor_format_e format;     /**< 数据格式 */
    tensor_memory_type_e memory_type; /**< 内存类型 */
    void* data;                 /**< 数据指针 */
    size_t size;                /**< 数据大小（字节） */
    bool owns_data;             /**< 是否拥有数据内存 */
    uint32_t ref_count;         /**< 引用计数 */
} tensor_t;

// 为了向后兼容，保留旧的类型别名
typedef tensor_data_type_e TensorDataType;
typedef tensor_format_e TensorFormat;
typedef tensor_memory_type_e TensorMemoryType;
typedef tensor_shape_t TensorShape;
typedef tensor_t Tensor;

/**
 * @brief 创建张量
 * 
 * @param name 张量名称
 * @param dtype 数据类型
 * @param shape 张量形状
 * @param format 数据格式
 * @return tensor_t 创建的张量
 */
tensor_t tensor_create(const char* name, tensor_data_type_e dtype, const tensor_shape_t* shape, tensor_format_e format);

/**
 * @brief 从现有数据创建张量
 * 
 * @param name 张量名称
 * @param dtype 数据类型
 * @param shape 张量形状
 * @param format 数据格式
 * @param data 数据指针
 * @param size 数据大小
 * @param owns_data 是否拥有数据内存
 * @return tensor_t 创建的张量
 */
tensor_t tensor_from_data(const char* name, tensor_data_type_e dtype, const tensor_shape_t* shape, 
                       tensor_format_e format, void* data, size_t size, bool owns_data);

/**
 * @brief 复制张量
 * 
 * @param src 源张量
 * @return tensor_t 复制的张量
 */
tensor_t tensor_copy(const tensor_t* src);

/**
 * @brief 释放张量
 * 
 * @param tensor 张量指针
 */
void tensor_free(tensor_t* tensor);

/**
 * @brief 获取张量元素数量
 * 
 * @param tensor 张量指针
 * @return uint32_t 元素数量
 */
uint32_t tensor_get_element_count(const tensor_t* tensor);

/**
 * @brief 获取数据类型大小
 * 
 * @param dtype 数据类型
 * @return size_t 数据类型大小（字节）
 */
size_t tensor_get_dtype_size(tensor_data_type_e dtype);

/**
 * @brief 重塑张量形状
 * 
 * @param tensor 张量指针
 * @param new_shape 新的形状
 * @return int 0表示成功，负数表示失败
 */
int tensor_reshape(tensor_t* tensor, const tensor_shape_t* new_shape);

/**
 * @brief 转换张量格式
 * 
 * @param tensor 张量指针
 * @param new_format 新的格式
 * @return int 0表示成功，负数表示失败
 */
int tensor_convert_format(tensor_t* tensor, tensor_format_e new_format);

/**
 * @brief 打印张量信息
 * 
 * @param tensor 张量指针
 */
void tensor_print_info(const tensor_t* tensor);

/**
 * @brief 创建张量形状
 * 
 * @param dims 维度数组
 * @param ndim 维度数量
 * @return tensor_shape_t 张量形状
 */
tensor_shape_t tensor_shape_create(const uint32_t* dims, uint32_t ndim);

/**
 * @brief 比较张量形状
 * 
 * @param shape1 形状1
 * @param shape2 形状2
 * @return bool 是否相等
 */
bool tensor_shape_equal(const tensor_shape_t* shape1, const tensor_shape_t* shape2);

/**
 * @brief 从图像创建张量
 * 
 * @param image_path 图像路径
 * @param target_shape 目标形状
 * @param format 目标格式
 * @return tensor_t 创建的张量
 */
tensor_t prepare_tensor_from_image(const char* image_path, const tensor_shape_t* target_shape, tensor_format_e format);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_TENSOR_H 