#ifndef MODYN_CORE_MULTIMODAL_H
#define MODYN_CORE_MULTIMODAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模态类型枚举
 */
typedef enum {
    MODALITY_UNKNOWN = 0,       /**< 未知模态 */
    MODALITY_TEXT,              /**< 文本模态 */
    MODALITY_IMAGE,             /**< 图像模态 */
    MODALITY_AUDIO,             /**< 音频模态 */
    MODALITY_VIDEO,             /**< 视频模态 */
    MODALITY_POINT_CLOUD,       /**< 点云模态 */
    MODALITY_DEPTH,             /**< 深度模态 */
    MODALITY_THERMAL,           /**< 热成像模态 */
    MODALITY_RADAR,             /**< 雷达模态 */
    MODALITY_LIDAR,             /**< 激光雷达模态 */
    MODALITY_SENSOR,            /**< 传感器模态 */
    MODALITY_CUSTOM             /**< 自定义模态 */
} modality_type_e;

/**
 * @brief 数据格式枚举
 */
typedef enum {
    DATA_FORMAT_UNKNOWN = 0,    /**< 未知格式 */
    
    // 文本格式
    DATA_FORMAT_UTF8,           /**< UTF-8文本 */
    DATA_FORMAT_ASCII,          /**< ASCII文本 */
    DATA_FORMAT_TOKEN,          /**< 令牌序列 */
    DATA_FORMAT_EMBEDDING,      /**< 嵌入向量 */
    
    // 图像格式
    DATA_FORMAT_RGB,            /**< RGB图像 */
    DATA_FORMAT_BGR,            /**< BGR图像 */
    DATA_FORMAT_RGBA,           /**< RGBA图像 */
    DATA_FORMAT_GRAY,           /**< 灰度图像 */
    DATA_FORMAT_YUV,            /**< YUV图像 */
    DATA_FORMAT_HSV,            /**< HSV图像 */
    DATA_FORMAT_JPEG,           /**< JPEG压缩 */
    DATA_FORMAT_PNG,            /**< PNG压缩 */
    
    // 音频格式
    DATA_FORMAT_PCM,            /**< PCM音频 */
    DATA_FORMAT_WAV,            /**< WAV音频 */
    DATA_FORMAT_MP3,            /**< MP3音频 */
    DATA_FORMAT_AAC,            /**< AAC音频 */
    DATA_FORMAT_FLAC,           /**< FLAC音频 */
    DATA_FORMAT_SPECTROGRAM,    /**< 频谱图 */
    DATA_FORMAT_MFCC,           /**< MFCC特征 */
    
    // 视频格式
    DATA_FORMAT_H264,           /**< H.264视频 */
    DATA_FORMAT_H265,           /**< H.265视频 */
    DATA_FORMAT_VP8,            /**< VP8视频 */
    DATA_FORMAT_VP9,            /**< VP9视频 */
    DATA_FORMAT_AV1,            /**< AV1视频 */
    
    // 3D格式
    DATA_FORMAT_PLY,            /**< PLY点云 */
    DATA_FORMAT_PCD,            /**< PCD点云 */
    DATA_FORMAT_OBJ,            /**< OBJ模型 */
    DATA_FORMAT_STL,            /**< STL模型 */
    
    DATA_FORMAT_CUSTOM          /**< 自定义格式 */
} data_format_e;

/**
 * @brief 模态数据结构
 */
typedef struct {
    modality_type_e modality;   /**< 模态类型 */
    data_format_e format;       /**< 数据格式 */
    void* data;                 /**< 数据指针 */
    size_t data_size;           /**< 数据大小 */
    TensorShape shape;          /**< 数据形状 */
    TensorDataType data_type;   /**< 数据类型 */
    char* metadata;             /**< 元数据（JSON格式） */
    uint64_t timestamp;         /**< 时间戳 */
    uint32_t sequence_id;       /**< 序列ID */
    char* source_id;            /**< 数据源ID */
} modality_data_t;

/**
 * @brief 多模态数据容器
 */
typedef struct {
    modality_data_t* modalities; /**< 模态数据数组 */
    uint32_t modality_count;    /**< 模态数量 */
    uint32_t capacity;          /**< 容量 */
    char* session_id;           /**< 会话ID */
    uint64_t created_time;      /**< 创建时间 */
    void* user_data;            /**< 用户数据 */
} multimodal_data_t;

/**
 * @brief 模态处理器句柄
 */
typedef struct ModalityProcessor* modality_processor_t;

/**
 * @brief 模态转换器句柄
 */
typedef struct ModalityConverter* modality_converter_t;

/**
 * @brief 模态融合器句柄
 */
typedef struct ModalityFuser* modality_fuser_t;

/**
 * @brief 模态处理函数类型
 */
typedef int (*modality_process_func_t)(const modality_data_t* input, modality_data_t* output, void* context);

/**
 * @brief 模态转换函数类型
 */
typedef int (*modality_convert_func_t)(const modality_data_t* input, data_format_e target_format, 
                                      modality_data_t* output, void* context);

/**
 * @brief 模态融合函数类型
 */
typedef int (*modality_fuse_func_t)(const modality_data_t* inputs, uint32_t input_count,
                                   modality_data_t* output, void* context);

// 为了向后兼容，保留旧的类型别名
typedef modality_type_e ModalityType;
typedef data_format_e DataFormat;
typedef modality_data_t ModalityData;
typedef multimodal_data_t MultiModalData;
typedef modality_processor_t ModalityProcessor;
typedef modality_converter_t ModalityConverter;
typedef modality_fuser_t ModalityFuser;
typedef modality_process_func_t ModalityProcessFunc;
typedef modality_convert_func_t ModalityConvertFunc;
typedef modality_fuse_func_t ModalityFuseFunc;

/**
 * @brief 创建模态数据
 * 
 * @param modality 模态类型
 * @param format 数据格式
 * @param data 数据指针
 * @param data_size 数据大小
 * @return ModalityData* 模态数据
 */
ModalityData* modality_data_create(ModalityType modality, DataFormat format, 
                                  const void* data, size_t data_size);

/**
 * @brief 销毁模态数据
 * 
 * @param modal_data 模态数据
 */
void modality_data_destroy(ModalityData* modal_data);

/**
 * @brief 复制模态数据
 * 
 * @param src 源数据
 * @return ModalityData* 复制的数据
 */
ModalityData* modality_data_copy(const ModalityData* src);

/**
 * @brief 创建多模态数据容器
 * 
 * @param capacity 初始容量
 * @return MultiModalData* 多模态数据容器
 */
MultiModalData* multimodal_data_create(uint32_t capacity);

/**
 * @brief 销毁多模态数据容器
 * 
 * @param multi_data 多模态数据容器
 */
void multimodal_data_destroy(MultiModalData* multi_data);

/**
 * @brief 添加模态数据
 * 
 * @param multi_data 多模态数据容器
 * @param modal_data 模态数据
 * @return int 0成功，其他失败
 */
int multimodal_data_add(MultiModalData* multi_data, const ModalityData* modal_data);

/**
 * @brief 获取模态数据
 * 
 * @param multi_data 多模态数据容器
 * @param modality 模态类型
 * @return ModalityData* 模态数据
 */
ModalityData* multimodal_data_get(const MultiModalData* multi_data, ModalityType modality);

/**
 * @brief 移除模态数据
 * 
 * @param multi_data 多模态数据容器
 * @param modality 模态类型
 * @return int 0成功，其他失败
 */
int multimodal_data_remove(MultiModalData* multi_data, ModalityType modality);

/**
 * @brief 创建模态处理器
 * 
 * @param modality 模态类型
 * @param process_func 处理函数
 * @param context 上下文
 * @return ModalityProcessor 模态处理器
 */
ModalityProcessor modality_processor_create(ModalityType modality, ModalityProcessFunc process_func, void* context);

/**
 * @brief 销毁模态处理器
 * 
 * @param processor 模态处理器
 */
void modality_processor_destroy(ModalityProcessor processor);

/**
 * @brief 处理模态数据
 * 
 * @param processor 模态处理器
 * @param input 输入数据
 * @param output 输出数据
 * @return int 0成功，其他失败
 */
int modality_processor_process(ModalityProcessor processor, const ModalityData* input, ModalityData* output);

/**
 * @brief 创建模态转换器
 * 
 * @param convert_func 转换函数
 * @param context 上下文
 * @return ModalityConverter 模态转换器
 */
ModalityConverter modality_converter_create(ModalityConvertFunc convert_func, void* context);

/**
 * @brief 销毁模态转换器
 * 
 * @param converter 模态转换器
 */
void modality_converter_destroy(ModalityConverter converter);

/**
 * @brief 转换模态数据格式
 * 
 * @param converter 模态转换器
 * @param input 输入数据
 * @param target_format 目标格式
 * @param output 输出数据
 * @return int 0成功，其他失败
 */
int modality_converter_convert(ModalityConverter converter, const ModalityData* input, 
                              DataFormat target_format, ModalityData* output);

/**
 * @brief 创建模态融合器
 * 
 * @param fuse_func 融合函数
 * @param context 上下文
 * @return ModalityFuser 模态融合器
 */
ModalityFuser modality_fuser_create(ModalityFuseFunc fuse_func, void* context);

/**
 * @brief 销毁模态融合器
 * 
 * @param fuser 模态融合器
 */
void modality_fuser_destroy(ModalityFuser fuser);

/**
 * @brief 融合多模态数据
 * 
 * @param fuser 模态融合器
 * @param inputs 输入数据数组
 * @param input_count 输入数量
 * @param output 输出数据
 * @return int 0成功，其他失败
 */
int modality_fuser_fuse(ModalityFuser fuser, const ModalityData* inputs, uint32_t input_count,
                       ModalityData* output);

/**
 * @brief 模态数据转换为张量
 * 
 * @param modal_data 模态数据
 * @param tensor 输出张量
 * @return int 0成功，其他失败
 */
int modality_data_to_tensor(const ModalityData* modal_data, Tensor* tensor);

/**
 * @brief 张量转换为模态数据
 * 
 * @param tensor 输入张量
 * @param modality 模态类型
 * @param format 数据格式
 * @param modal_data 输出模态数据
 * @return int 0成功，其他失败
 */
int tensor_to_modality_data(const Tensor* tensor, ModalityType modality, DataFormat format,
                           ModalityData* modal_data);

/**
 * @brief 获取模态类型名称
 * 
 * @param modality 模态类型
 * @return const char* 模态名称
 */
const char* modality_type_to_string(ModalityType modality);

/**
 * @brief 从字符串解析模态类型
 * 
 * @param modality_str 模态字符串
 * @return ModalityType 模态类型
 */
ModalityType modality_type_from_string(const char* modality_str);

/**
 * @brief 获取数据格式名称
 * 
 * @param format 数据格式
 * @return const char* 格式名称
 */
const char* data_format_to_string(DataFormat format);

/**
 * @brief 从字符串解析数据格式
 * 
 * @param format_str 格式字符串
 * @return DataFormat 数据格式
 */
DataFormat data_format_from_string(const char* format_str);

/**
 * @brief 检查模态兼容性
 * 
 * @param modality1 模态1
 * @param modality2 模态2
 * @return bool true兼容，false不兼容
 */
bool modality_is_compatible(ModalityType modality1, ModalityType modality2);

/**
 * @brief 获取模态数据大小
 * 
 * @param modal_data 模态数据
 * @return size_t 数据大小
 */
size_t modality_data_get_size(const ModalityData* modal_data);

/**
 * @brief 验证模态数据
 * 
 * @param modal_data 模态数据
 * @return bool true有效，false无效
 */
bool modality_data_validate(const ModalityData* modal_data);

/**
 * @brief 序列化模态数据
 * 
 * @param modal_data 模态数据
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return int 序列化的字节数，失败返回-1
 */
int modality_data_serialize(const ModalityData* modal_data, void* buffer, size_t buffer_size);

/**
 * @brief 反序列化模态数据
 * 
 * @param buffer 输入缓冲区
 * @param buffer_size 缓冲区大小
 * @param modal_data 输出模态数据
 * @return int 0成功，其他失败
 */
int modality_data_deserialize(const void* buffer, size_t buffer_size, ModalityData* modal_data);

/**
 * @brief 模态数据压缩
 * 
 * @param modal_data 模态数据
 * @param compressed 压缩后的数据
 * @return int 0成功，其他失败
 */
int modality_data_compress(const ModalityData* modal_data, ModalityData* compressed);

/**
 * @brief 模态数据解压缩
 * 
 * @param compressed 压缩的数据
 * @param modal_data 解压缩后的数据
 * @return int 0成功，其他失败
 */
int modality_data_decompress(const ModalityData* compressed, ModalityData* modal_data);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_MULTIMODAL_H 