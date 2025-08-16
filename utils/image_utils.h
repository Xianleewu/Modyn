#ifndef MODYN_UTILS_IMAGE_UTILS_H
#define MODYN_UTILS_IMAGE_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 图像数据类型
 */
typedef enum {
    IMAGE_DTYPE_UNKNOWN = 0,
    IMAGE_DTYPE_UINT8,
    IMAGE_DTYPE_INT8,
    IMAGE_DTYPE_UINT16,
    IMAGE_DTYPE_INT16,
    IMAGE_DTYPE_FLOAT32
} image_dtype_e;

/**
 * @brief 图像信息结构
 */
typedef struct {
    uint32_t width;             /**< 图像宽度 */
    uint32_t height;            /**< 图像高度 */
    uint32_t channels;          /**< 通道数 */
    image_dtype_e dtype;        /**< 数据类型 */
    size_t size;                /**< 数据大小 */
    bool valid;                 /**< 是否有效 */
} image_info_t;

/**
 * @brief 图像处理配置
 */
typedef struct {
    uint32_t target_width;      /**< 目标宽度 */
    uint32_t target_height;     /**< 目标高度 */
    uint32_t target_channels;   /**< 目标通道数 */
    TensorFormat format;        /**< 张量格式 */
    bool normalize;             /**< 是否归一化 */
    bool keep_aspect_ratio;     /**< 是否保持宽高比 */
    float mean[4];              /**< 均值 */
    float std[4];               /**< 标准差 */
} image_process_config_t;

// 为了向后兼容，保留旧的类型别名
typedef image_dtype_e ImageDataType;
typedef image_info_t ImageInfo;
typedef image_process_config_t ImageProcessConfig;

/**
 * @brief 获取图像信息
 * 
 * @param image_path 图像文件路径
 * @return ImageInfo 图像信息
 */
ImageInfo image_utils_get_info(const char* image_path);

/**
 * @brief 加载图像为张量
 * 
 * @param image_path 图像文件路径
 * @param config 处理配置
 * @return Tensor 创建的张量
 */
Tensor image_utils_load_tensor(const char* image_path, const ImageProcessConfig* config);

/**
 * @brief 保存张量为图像
 * 
 * @param tensor 张量指针
 * @param output_path 输出图像路径
 * @return int 0表示成功，负数表示失败
 */
int image_utils_save_tensor(const Tensor* tensor, const char* output_path);

/**
 * @brief 调整图像大小
 * 
 * @param input 输入张量
 * @param output 输出张量
 * @param new_width 新宽度
 * @param new_height 新高度
 * @return int 0表示成功，负数表示失败
 */
int image_utils_resize(const Tensor* input, Tensor* output, uint32_t new_width, uint32_t new_height);

/**
 * @brief 归一化张量
 * 
 * @param tensor 张量指针
 * @param mean 均值数组
 * @param std 标准差数组
 * @param channels 通道数
 * @return int 0表示成功，负数表示失败
 */
int image_utils_normalize(Tensor* tensor, const float* mean, const float* std, uint32_t channels);

/**
 * @brief 创建图像处理配置
 * 
 * @param width 目标宽度
 * @param height 目标高度
 * @param channels 目标通道数
 * @param format 张量格式
 * @param normalize 是否归一化
 * @return ImageProcessConfig 配置结构
 */
ImageProcessConfig image_utils_create_config(uint32_t width, uint32_t height, uint32_t channels, 
                                           TensorFormat format, bool normalize);

#ifdef __cplusplus
}
#endif

#endif // MODYN_UTILS_IMAGE_UTILS_H 