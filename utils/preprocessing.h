#ifndef MODYN_UTILS_PREPROCESSING_H
#define MODYN_UTILS_PREPROCESSING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/tensor.h"
#include "core/multimodal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 预处理操作类型
 */
typedef enum {
    PREPROCESS_UNKNOWN = 0,
    
    // 通用操作
    PREPROCESS_NORMALIZE,           /**< 归一化 */
    PREPROCESS_STANDARDIZE,         /**< 标准化 */
    PREPROCESS_QUANTIZE,            /**< 量化 */
    PREPROCESS_DEQUANTIZE,          /**< 反量化 */
    PREPROCESS_CAST,                /**< 类型转换 */
    PREPROCESS_TRANSPOSE,           /**< 转置 */
    PREPROCESS_RESHAPE,             /**< 重塑 */
    PREPROCESS_PAD,                 /**< 填充 */
    PREPROCESS_CROP,                /**< 裁剪 */
    
    // 图像操作
    PREPROCESS_RESIZE,              /**< 调整大小 */
    PREPROCESS_ROTATE,              /**< 旋转 */
    PREPROCESS_FLIP,                /**< 翻转 */
    PREPROCESS_COLOR_CONVERT,       /**< 颜色空间转换 */
    PREPROCESS_BRIGHTNESS,          /**< 亮度调整 */
    PREPROCESS_CONTRAST,            /**< 对比度调整 */
    PREPROCESS_GAMMA,               /**< 伽马校正 */
    PREPROCESS_BLUR,                /**< 模糊 */
    PREPROCESS_SHARPEN,             /**< 锐化 */
    PREPROCESS_EDGE_DETECT,         /**< 边缘检测 */
    PREPROCESS_MORPHOLOGY,          /**< 形态学操作 */
    PREPROCESS_HISTOGRAM_EQ,        /**< 直方图均衡化 */
    
    // 音频操作
    PREPROCESS_RESAMPLE,            /**< 重采样 */
    PREPROCESS_AMPLITUDE_SCALE,     /**< 幅度缩放 */
    PREPROCESS_NOISE_REDUCTION,     /**< 降噪 */
    PREPROCESS_FILTER,              /**< 滤波 */
    PREPROCESS_WINDOWING,           /**< 加窗 */
    PREPROCESS_FFT,                 /**< 快速傅里叶变换 */
    PREPROCESS_MFCC,                /**< MFCC特征提取 */
    PREPROCESS_SPECTROGRAM,         /**< 频谱图生成 */
    PREPROCESS_PITCH_SHIFT,         /**< 音调变换 */
    PREPROCESS_TIME_STRETCH,        /**< 时间拉伸 */
    
    // 文本操作
    PREPROCESS_TOKENIZE,            /**< 分词 */
    PREPROCESS_ENCODE,              /**< 编码 */
    PREPROCESS_DECODE,              /**< 解码 */
    PREPROCESS_EMBEDDING,           /**< 嵌入 */
    PREPROCESS_SEQUENCE_PAD,        /**< 序列填充 */
    PREPROCESS_SEQUENCE_TRUNCATE,   /**< 序列截断 */
    PREPROCESS_MASK,                /**< 掩码 */
    
    // 3D点云操作
    PREPROCESS_DOWNSAMPLE,          /**< 下采样 */
    PREPROCESS_UPSAMPLE,            /**< 上采样 */
    PREPROCESS_OUTLIER_REMOVAL,     /**< 离群点移除 */
    PREPROCESS_SMOOTH,              /**< 平滑 */
    PREPROCESS_NORMAL_ESTIMATION,   /**< 法向量估计 */
    PREPROCESS_REGISTRATION,        /**< 配准 */
    PREPROCESS_SEGMENTATION,        /**< 分割 */
    
    PREPROCESS_CUSTOM               /**< 自定义操作 */
} preprocess_type_e;

/**
 * @brief 插值方法
 */
typedef enum {
    INTERPOLATION_NEAREST = 0,      /**< 最近邻插值 */
    INTERPOLATION_LINEAR,           /**< 线性插值 */
    INTERPOLATION_CUBIC,            /**< 三次插值 */
    INTERPOLATION_LANCZOS,          /**< Lanczos插值 */
    INTERPOLATION_AREA              /**< 区域插值 */
} interpolation_method_e;

/**
 * @brief 填充模式
 */
typedef enum {
    PADDING_CONSTANT = 0,           /**< 常数填充 */
    PADDING_REFLECT,                /**< 反射填充 */
    PADDING_REPLICATE,              /**< 重复填充 */
    PADDING_WRAP,                   /**< 环绕填充 */
    PADDING_SYMMETRIC               /**< 对称填充 */
} padding_mode_e;

/**
 * @brief 预处理参数
 */
typedef struct {
    preprocess_type_e type;         /**< 操作类型 */
    
    union {
        struct {
            float mean[4];          /**< 均值 */
            float std[4];           /**< 标准差 */
            uint32_t channels;      /**< 通道数 */
        } normalize;
        
        struct {
            uint32_t width;         /**< 宽度 */
            uint32_t height;        /**< 高度 */
            interpolation_method_e method; /**< 插值方法 */
        } resize;
        
        struct {
            float angle;            /**< 角度 */
            float center_x;         /**< 旋转中心X */
            float center_y;         /**< 旋转中心Y */
            float scale;            /**< 缩放比例 */
        } rotate;
        
        struct {
            bool horizontal;        /**< 水平翻转 */
            bool vertical;          /**< 垂直翻转 */
        } flip;
        
        struct {
            uint32_t top;           /**< 上边距 */
            uint32_t bottom;        /**< 下边距 */
            uint32_t left;          /**< 左边距 */
            uint32_t right;         /**< 右边距 */
            padding_mode_e mode;    /**< 填充模式 */
            float value;            /**< 填充值 */
        } pad;
        
        struct {
            uint32_t x;             /**< 起始X */
            uint32_t y;             /**< 起始Y */
            uint32_t width;         /**< 宽度 */
            uint32_t height;        /**< 高度 */
        } crop;
        
        struct {
            float factor;           /**< 调整因子 */
        } brightness;
        
        struct {
            float factor;           /**< 调整因子 */
        } contrast;
        
        struct {
            float gamma;            /**< 伽马值 */
        } gamma;
        
        struct {
            uint32_t kernel_size;   /**< 核大小 */
            float sigma;            /**< 标准差 */
        } blur;
        
        struct {
            uint32_t sample_rate;   /**< 采样率 */
            uint32_t target_rate;   /**< 目标采样率 */
        } resample;
        
        struct {
            float scale;            /**< 缩放因子 */
        } amplitude_scale;
        
        struct {
            uint32_t window_size;   /**< 窗口大小 */
            uint32_t hop_size;      /**< 跳跃大小 */
        } windowing;
        
        struct {
            uint32_t n_fft;         /**< FFT点数 */
            uint32_t hop_length;    /**< 跳跃长度 */
            uint32_t win_length;    /**< 窗口长度 */
        } fft;
        
        struct {
            uint32_t n_mfcc;        /**< MFCC系数数量 */
            uint32_t n_fft;         /**< FFT点数 */
            uint32_t hop_length;    /**< 跳跃长度 */
        } mfcc;
        
        struct {
            uint32_t max_length;    /**< 最大长度 */
            uint32_t pad_value;     /**< 填充值 */
        } sequence_pad;
        
        struct {
            uint32_t max_length;    /**< 最大长度 */
        } sequence_truncate;
        
        struct {
            uint32_t target_points; /**< 目标点数 */
        } downsample;
        
        struct {
            uint32_t k;             /**< 邻居数 */
            float std_ratio;        /**< 标准差比例 */
        } outlier_removal;
        
        struct {
            uint32_t iterations;    /**< 迭代次数 */
            float radius;           /**< 平滑半径 */
        } smooth;
        
        struct {
            uint32_t k;             /**< 邻居数 */
            float radius;           /**< 搜索半径 */
        } normal_estimation;
        
        struct {
            void* custom_data;      /**< 自定义数据 */
            size_t data_size;       /**< 数据大小 */
        } custom;
    } params;
} preprocess_params_t;

/**
 * @brief 预处理操作句柄
 */
typedef struct PreprocessOp* preprocess_op_t;

/**
 * @brief 预处理管道句柄
 */
typedef struct PreprocessPipeline* preprocess_pipeline_t;

/**
 * @brief 自定义预处理函数类型
 */
typedef int (*custom_preprocess_func_t)(const Tensor* input, Tensor* output, const void* params, void* context);

/**
 * @brief 创建预处理操作
 * 
 * @param type 操作类型
 * @param params 参数
 * @return PreprocessOp 预处理操作
 */
preprocess_op_t preprocess_op_create(preprocess_type_e type, const preprocess_params_t* params);

/**
 * @brief 销毁预处理操作
 * 
 * @param op 预处理操作
 */
void preprocess_op_destroy(preprocess_op_t op);

/**
 * @brief 执行预处理操作
 * 
 * @param op 预处理操作
 * @param input 输入张量
 * @param output 输出张量
 * @return int 0成功，其他失败
 */
int preprocess_op_execute(preprocess_op_t op, const Tensor* input, Tensor* output);

/**
 * @brief 创建预处理管道
 * 
 * @return PreprocessPipeline 预处理管道
 */
preprocess_pipeline_t preprocess_pipeline_create(void);

/**
 * @brief 销毁预处理管道
 * 
 * @param pipeline 预处理管道
 */
void preprocess_pipeline_destroy(preprocess_pipeline_t pipeline);

/**
 * @brief 添加预处理操作到管道
 * 
 * @param pipeline 预处理管道
 * @param op 预处理操作
 * @return int 0成功，其他失败
 */
int preprocess_pipeline_add_op(preprocess_pipeline_t pipeline, preprocess_op_t op);

/**
 * @brief 移除预处理操作
 * 
 * @param pipeline 预处理管道
 * @param index 操作索引
 * @return int 0成功，其他失败
 */
int preprocess_pipeline_remove_op(preprocess_pipeline_t pipeline, uint32_t index);

/**
 * @brief 执行预处理管道
 * 
 * @param pipeline 预处理管道
 * @param input 输入张量
 * @param output 输出张量
 * @return int 0成功，其他失败
 */
int preprocess_pipeline_execute(preprocess_pipeline_t pipeline, const Tensor* input, Tensor* output);

/**
 * @brief 批量执行预处理管道
 * 
 * @param pipeline 预处理管道
 * @param inputs 输入张量数组
 * @param outputs 输出张量数组
 * @param batch_size 批量大小
 * @return int 0成功，其他失败
 */
int preprocess_pipeline_execute_batch(preprocess_pipeline_t pipeline, const Tensor* inputs, 
                                     Tensor* outputs, uint32_t batch_size);

/**
 * @brief 获取管道操作数量
 * 
 * @param pipeline 预处理管道
 * @return uint32_t 操作数量
 */
uint32_t preprocess_pipeline_get_op_count(preprocess_pipeline_t pipeline);

/**
 * @brief 获取管道操作
 * 
 * @param pipeline 预处理管道
 * @param index 操作索引
 * @return PreprocessOp 预处理操作
 */
preprocess_op_t preprocess_pipeline_get_op(preprocess_pipeline_t pipeline, uint32_t index);

/**
 * @brief 克隆预处理管道
 * 
 * @param pipeline 源管道
 * @return PreprocessPipeline 克隆的管道
 */
preprocess_pipeline_t preprocess_pipeline_clone(preprocess_pipeline_t pipeline);

/**
 * @brief 序列化预处理管道
 * 
 * @param pipeline 预处理管道
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return int 序列化的字节数，失败返回-1
 */
int preprocess_pipeline_serialize(preprocess_pipeline_t pipeline, void* buffer, size_t buffer_size);

/**
 * @brief 反序列化预处理管道
 * 
 * @param buffer 输入缓冲区
 * @param buffer_size 缓冲区大小
 * @return PreprocessPipeline 反序列化的管道
 */
preprocess_pipeline_t preprocess_pipeline_deserialize(const void* buffer, size_t buffer_size);

/**
 * @brief 多模态预处理
 * 
 * @param pipeline 预处理管道
 * @param input 输入多模态数据
 * @param output 输出多模态数据
 * @return int 0成功，其他失败
 */
int preprocess_multimodal_execute(preprocess_pipeline_t pipeline, const MultiModalData* input, 
                                 MultiModalData* output);

/**
 * @brief 创建自定义预处理操作
 * 
 * @param func 自定义函数
 * @param params 参数
 * @param params_size 参数大小
 * @param context 上下文
 * @return PreprocessOp 预处理操作
 */
preprocess_op_t preprocess_op_create_custom(custom_preprocess_func_t func, const void* params, 
                                        size_t params_size, void* context);

/**
 * @brief 注册预处理操作
 * 
 * @param type 操作类型
 * @param func 处理函数
 * @return int 0成功，其他失败
 */
int preprocess_register_op(preprocess_type_e type, custom_preprocess_func_t func);

/**
 * @brief 获取预处理操作名称
 * 
 * @param type 操作类型
 * @return const char* 操作名称
 */
const char* preprocess_type_to_string(preprocess_type_e type);

/**
 * @brief 从字符串解析预处理操作类型
 * 
 * @param type_str 类型字符串
 * @return PreprocessType 操作类型
 */
preprocess_type_e preprocess_type_from_string(const char* type_str);

/**
 * @brief 验证预处理参数
 * 
 * @param type 操作类型
 * @param params 参数
 * @return bool true有效，false无效
 */
bool preprocess_validate_params(preprocess_type_e type, const preprocess_params_t* params);

/**
 * @brief 获取预处理操作的输出形状
 * 
 * @param op 预处理操作
 * @param input_shape 输入形状
 * @param output_shape 输出形状
 * @return int 0成功，其他失败
 */
int preprocess_op_get_output_shape(preprocess_op_t op, const TensorShape* input_shape, 
                                  TensorShape* output_shape);

/**
 * @brief 预处理操作性能分析
 * 
 * @param op 预处理操作
 * @param input 输入张量
 * @param iterations 迭代次数
 * @param avg_time 平均时间（毫秒）
 * @return int 0成功，其他失败
 */
int preprocess_op_benchmark(preprocess_op_t op, const Tensor* input, uint32_t iterations, 
                           double* avg_time);

/**
 * @brief 预处理管道性能分析
 * 
 * @param pipeline 预处理管道
 * @param input 输入张量
 * @param iterations 迭代次数
 * @param avg_time 平均时间（毫秒）
 * @return int 0成功，其他失败
 */
int preprocess_pipeline_benchmark(preprocess_pipeline_t pipeline, const Tensor* input, 
                                 uint32_t iterations, double* avg_time);

/**
 * @brief 预处理操作缓存
 * 
 * @param op 预处理操作
 * @param enable 是否启用缓存
 * @return int 0成功，其他失败
 */
int preprocess_op_set_cache(preprocess_op_t op, bool enable);

/**
 * @brief 预处理管道并行化
 * 
 * @param pipeline 预处理管道
 * @param num_threads 线程数
 * @return int 0成功，其他失败
 */
int preprocess_pipeline_set_parallel(preprocess_pipeline_t pipeline, uint32_t num_threads);

/**
 * @brief 预处理操作调试信息
 * 
 * @param op 预处理操作
 */
void preprocess_op_print_debug(preprocess_op_t op);

/**
 * @brief 预处理管道调试信息
 * 
 * @param pipeline 预处理管道
 */
void preprocess_pipeline_print_debug(preprocess_pipeline_t pipeline);

#ifdef __cplusplus
}
#endif

#endif // MODYN_UTILS_PREPROCESSING_H 