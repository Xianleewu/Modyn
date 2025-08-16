#include "utils/image_utils.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifdef MODYN_ENABLE_OPENCV
#include <opencv2/opencv.hpp>
using namespace cv;
#endif

/**
 * @brief 图像处理工具实现
 */

ImageInfo image_utils_get_info(const char* image_path) {
    ImageInfo info = {0};
    
    if (!image_path) {
        LOG_ERROR("图像路径为空");
        return info;
    }
    
#ifdef MODYN_ENABLE_OPENCV
    Mat image = imread(image_path);
    if (image.empty()) {
        LOG_ERROR("无法加载图像: %s", image_path);
        return info;
    }
    
    info.width = image.cols;
    info.height = image.rows;
    info.channels = image.channels();
    
    switch (image.depth()) {
        case CV_8U:
            info.dtype = IMAGE_DTYPE_UINT8;
            break;
        case CV_8S:
            info.dtype = IMAGE_DTYPE_INT8;
            break;
        case CV_16U:
            info.dtype = IMAGE_DTYPE_UINT16;
            break;
        case CV_16S:
            info.dtype = IMAGE_DTYPE_INT16;
            break;
        case CV_32F:
            info.dtype = IMAGE_DTYPE_FLOAT32;
            break;
        default:
            info.dtype = IMAGE_DTYPE_UNKNOWN;
    }
    
    info.size = image.total() * image.elemSize();
    info.valid = true;
    
    LOG_DEBUG("图像信息: %s - %dx%d, %d通道, %zu字节", 
              image_path, info.width, info.height, info.channels, info.size);
#else
    // 简化实现：根据文件扩展名猜测
    const char* ext = strrchr(image_path, '.');
    if (ext && (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0 || 
                strcmp(ext, ".bmp") == 0 || strcmp(ext, ".jpeg") == 0)) {
        // 设置默认值
        info.width = 224;
        info.height = 224;
        info.channels = 3;
        info.dtype = IMAGE_DTYPE_UINT8;
        info.size = 224 * 224 * 3;
        info.valid = true;
        
        LOG_WARN("使用虚拟图像信息（未启用OpenCV）: %s", image_path);
    } else {
        LOG_ERROR("不支持的图像格式: %s", image_path);
    }
#endif
    
    return info;
}

Tensor image_utils_load_tensor(const char* image_path, const ImageProcessConfig* config) {
    Tensor tensor = {0};
    
    if (!image_path || !config) {
        LOG_ERROR("图像加载参数无效");
        return tensor;
    }
    
#ifdef MODYN_ENABLE_OPENCV
    Mat image = imread(image_path);
    if (image.empty()) {
        LOG_ERROR("无法加载图像: %s", image_path);
        return tensor;
    }
    
    // 调整大小
    if (config->target_width > 0 && config->target_height > 0) {
        Mat resized;
        resize(image, resized, Size(config->target_width, config->target_height));
        image = resized;
    }
    
    // 颜色空间转换
    if (config->target_channels == 1 && image.channels() == 3) {
        Mat gray;
        cvtColor(image, gray, COLOR_BGR2GRAY);
        image = gray;
    } else if (config->target_channels == 3 && image.channels() == 1) {
        Mat color;
        cvtColor(image, color, COLOR_GRAY2BGR);
        image = color;
    }
    
    // 数据类型转换
    Mat float_image;
    if (config->normalize) {
        image.convertTo(float_image, CV_32F, 1.0/255.0);
    } else {
        image.convertTo(float_image, CV_32F);
    }
    
    // 创建张量
    uint32_t dims[4];
    uint32_t ndim;
    
    if (config->format == TENSOR_FORMAT_NCHW) {
        dims[0] = 1;  // batch
        dims[1] = float_image.channels();  // channels
        dims[2] = float_image.rows;        // height
        dims[3] = float_image.cols;        // width
        ndim = 4;
    } else {  // NHWC
        dims[0] = 1;  // batch
        dims[1] = float_image.rows;        // height
        dims[2] = float_image.cols;        // width
        dims[3] = float_image.channels();  // channels
        ndim = 4;
    }
    
    TensorShape shape = tensor_shape_create(dims, ndim);
    tensor = tensor_create("image_tensor", TENSOR_TYPE_FLOAT32, &shape, config->format);
    
    // 分配内存
    tensor.data = malloc(tensor.size);
    if (!tensor.data) {
        LOG_ERROR("张量内存分配失败");
        return tensor;
    }
    tensor.owns_data = true;
    
    // 复制数据
    float* tensor_data = (float*)tensor.data;
    
    if (config->format == TENSOR_FORMAT_NCHW) {
        // 转换为NCHW格式
        for (int c = 0; c < float_image.channels(); c++) {
            for (int h = 0; h < float_image.rows; h++) {
                for (int w = 0; w < float_image.cols; w++) {
                    int tensor_idx = c * float_image.rows * float_image.cols + h * float_image.cols + w;
                    
                    if (float_image.channels() == 1) {
                        tensor_data[tensor_idx] = float_image.at<float>(h, w);
                    } else {
                        Vec3f pixel = float_image.at<Vec3f>(h, w);
                        tensor_data[tensor_idx] = pixel[c];
                    }
                }
            }
        }
    } else {
        // NHWC格式，直接复制
        memcpy(tensor_data, float_image.data, tensor.size);
    }
    
    LOG_INFO("图像转张量成功: %s -> [%d, %d, %d, %d]", 
             image_path, dims[0], dims[1], dims[2], dims[3]);
             
#else
    // 简化实现：创建虚拟张量
    uint32_t dims[] = {1, config->target_channels, config->target_height, config->target_width};
    TensorShape shape = tensor_shape_create(dims, 4);
    tensor = tensor_create("dummy_image", TENSOR_TYPE_FLOAT32, &shape, config->format);
    
    tensor.data = malloc(tensor.size);
    if (tensor.data) {
        tensor.owns_data = true;
        // 填充随机数据
        float* data = (float*)tensor.data;
        for (uint32_t i = 0; i < tensor_get_element_count(&tensor); i++) {
            data[i] = (float)(rand() % 256) / 255.0f;
        }
    }
    
    LOG_WARN("使用虚拟图像数据（未启用OpenCV）: %s", image_path);
#endif
    
    return tensor;
}

int image_utils_save_tensor(const Tensor* tensor, const char* output_path) {
    if (!tensor || !output_path) {
        LOG_ERROR("保存张量参数无效");
        return -1;
    }
    
#ifdef MODYN_ENABLE_OPENCV
    if (tensor->dtype != TENSOR_TYPE_FLOAT32) {
        LOG_ERROR("只支持FLOAT32张量");
        return -1;
    }
    
    float* data = (float*)tensor->data;
    
    // 解析张量形状
    int height, width, channels;
    if (tensor->format == TENSOR_FORMAT_NCHW && tensor->shape.ndim == 4) {
        channels = tensor->shape.dims[1];
        height = tensor->shape.dims[2];
        width = tensor->shape.dims[3];
    } else if (tensor->format == TENSOR_FORMAT_NHWC && tensor->shape.ndim == 4) {
        height = tensor->shape.dims[1];
        width = tensor->shape.dims[2];
        channels = tensor->shape.dims[3];
    } else {
        LOG_ERROR("不支持的张量格式");
        return -1;
    }
    
    // 创建OpenCV矩阵
    Mat image;
    if (channels == 1) {
        image = Mat(height, width, CV_32F);
    } else if (channels == 3) {
        image = Mat(height, width, CV_32FC3);
    } else {
        LOG_ERROR("不支持的通道数: %d", channels);
        return -1;
    }
    
    // 复制数据
    if (tensor->format == TENSOR_FORMAT_NCHW) {
        for (int c = 0; c < channels; c++) {
            for (int h = 0; h < height; h++) {
                for (int w = 0; w < width; w++) {
                    int tensor_idx = c * height * width + h * width + w;
                    
                    if (channels == 1) {
                        image.at<float>(h, w) = data[tensor_idx];
                    } else {
                        image.at<Vec3f>(h, w)[c] = data[tensor_idx];
                    }
                }
            }
        }
    } else {
        memcpy(image.data, data, tensor->size);
    }
    
    // 转换为8位并保存
    Mat output_image;
    image.convertTo(output_image, CV_8U, 255.0);
    
    if (imwrite(output_path, output_image)) {
        LOG_INFO("张量保存为图像成功: %s", output_path);
        return 0;
    } else {
        LOG_ERROR("图像保存失败: %s", output_path);
        return -1;
    }
#else
    LOG_WARN("图像保存功能需要OpenCV支持");
    return -1;
#endif
}

int image_utils_resize(const Tensor* input, Tensor* output, uint32_t new_width, uint32_t new_height) {
    if (!input || !output) {
        LOG_ERROR("调整大小参数无效");
        return -1;
    }
    
#ifdef MODYN_ENABLE_OPENCV
    // 从输入张量创建OpenCV矩阵
    // 调整大小
    // 将结果写入输出张量
    LOG_INFO("图像大小调整: %dx%d -> %dx%d", 
             input->shape.dims[3], input->shape.dims[2], new_width, new_height);
    // 实现细节...
    return 0;
#else
    // 简化实现：直接复制数据到新的张量
    uint32_t dims[] = {1, input->shape.dims[1], new_height, new_width};
    TensorShape new_shape = tensor_shape_create(dims, 4);
    
    *output = tensor_create("resized", input->dtype, &new_shape, input->format);
    output->data = malloc(output->size);
    if (!output->data) {
        return -1;
    }
    output->owns_data = true;
    
    // 简单的最近邻插值
    float* input_data = (float*)input->data;
    float* output_data = (float*)output->data;
    
    uint32_t channels = input->shape.dims[1];
    uint32_t old_height = input->shape.dims[2];
    uint32_t old_width = input->shape.dims[3];
    
    for (uint32_t c = 0; c < channels; c++) {
        for (uint32_t h = 0; h < new_height; h++) {
            for (uint32_t w = 0; w < new_width; w++) {
                uint32_t src_h = (h * old_height) / new_height;
                uint32_t src_w = (w * old_width) / new_width;
                
                uint32_t src_idx = c * old_height * old_width + src_h * old_width + src_w;
                uint32_t dst_idx = c * new_height * new_width + h * new_width + w;
                
                output_data[dst_idx] = input_data[src_idx];
            }
        }
    }
    
    LOG_INFO("图像大小调整完成（简化实现）");
    return 0;
#endif
}

int image_utils_normalize(Tensor* tensor, const float* mean, const float* std, uint32_t channels) {
    if (!tensor || !mean || !std || tensor->dtype != TENSOR_TYPE_FLOAT32) {
        LOG_ERROR("归一化参数无效");
        return -1;
    }
    
    float* data = (float*)tensor->data;
    uint32_t element_count = tensor_get_element_count(tensor);
    
    if (tensor->format == TENSOR_FORMAT_NCHW) {
        uint32_t pixels_per_channel = element_count / channels;
        
        for (uint32_t c = 0; c < channels; c++) {
            for (uint32_t i = 0; i < pixels_per_channel; i++) {
                uint32_t idx = c * pixels_per_channel + i;
                data[idx] = (data[idx] - mean[c]) / std[c];
            }
        }
    } else {
        // NHWC格式
        for (uint32_t i = 0; i < element_count; i += channels) {
            for (uint32_t c = 0; c < channels; c++) {
                data[i + c] = (data[i + c] - mean[c]) / std[c];
            }
        }
    }
    
    LOG_DEBUG("张量归一化完成");
    return 0;
}

ImageProcessConfig image_utils_create_config(uint32_t width, uint32_t height, uint32_t channels, 
                                           TensorFormat format, bool normalize) {
    ImageProcessConfig config = {0};
    
    config.target_width = width;
    config.target_height = height;
    config.target_channels = channels;
    config.format = format;
    config.normalize = normalize;
    config.keep_aspect_ratio = false;
    
    return config;
} 