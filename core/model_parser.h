#ifndef MODYN_CORE_MODEL_PARSER_H
#define MODYN_CORE_MODEL_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include "core/tensor.h"
#include "core/inference_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模型格式枚举
 */
typedef enum {
    MODEL_FORMAT_UNKNOWN = 0,
    MODEL_FORMAT_ONNX,          /**< ONNX 格式 */
    MODEL_FORMAT_RKNN,          /**< RKNN 格式 */
    MODEL_FORMAT_OPENVINO,      /**< OpenVINO 格式 */
    MODEL_FORMAT_TENSORRT,      /**< TensorRT 格式 */
    MODEL_FORMAT_PYTORCH,       /**< PyTorch 格式 */
    MODEL_FORMAT_TENSORFLOW,    /**< TensorFlow 格式 */
    MODEL_FORMAT_TFLITE         /**< TensorFlow Lite 格式 */
} model_format_e;

/**
 * @brief 模型输入输出描述
 */
typedef struct {
    char* name;                 /**< 输入/输出名称 */
    TensorShape shape;          /**< 张量形状 */
    TensorDataType data_type;   /**< 数据类型 */
    bool is_dynamic;            /**< 是否动态形状 */
    TensorShape min_shape;      /**< 最小形状（动态时） */
    TensorShape max_shape;      /**< 最大形状（动态时） */
    float scale;                /**< 量化缩放因子 */
    int32_t zero_point;         /**< 量化零点 */
    char* description;          /**< 描述信息 */
} model_io_spec_t;

/**
 * @brief 模型元信息
 */
typedef struct {
    char* name;                 /**< 模型名称 */
    char* version;              /**< 模型版本 */
    char* description;          /**< 模型描述 */
    char* author;               /**< 作者 */
    char* license;              /**< 许可证 */
    
    model_format_e format;      /**< 模型格式 */
    InferBackendType preferred_backend; /**< 推荐后端 */
    
    uint32_t input_count;       /**< 输入数量 */
    model_io_spec_t* inputs;    /**< 输入规格 */
    
    uint32_t output_count;      /**< 输出数量 */
    model_io_spec_t* outputs;   /**< 输出规格 */
    
    uint64_t model_size;        /**< 模型大小 */
    uint64_t memory_required;   /**< 所需内存 */
    
    bool supports_batching;     /**< 是否支持批处理 */
    uint32_t max_batch_size;    /**< 最大批处理大小 */
    
    void* custom_metadata;      /**< 自定义元数据 */
} model_metadata_t;

/**
 * @brief 模型解析器句柄
 */
typedef struct ModelParser* model_parser_t;

/**
 * @brief 创建模型解析器
 * 
 * @return model_parser_t 解析器实例
 */
model_parser_t model_parser_create(void);

/**
 * @brief 销毁模型解析器
 * 
 * @param parser 解析器实例
 */
void model_parser_destroy(model_parser_t parser);

/**
 * @brief 检测模型格式
 * 
 * @param model_path 模型文件路径
 * @return model_format_e 检测到的格式
 */
model_format_e model_parser_detect_format(const char* model_path);

/**
 * @brief 解析模型元信息
 * 
 * @param parser 解析器实例
 * @param model_path 模型文件路径
 * @param metadata 输出的元信息
 * @return int 0成功，其他失败
 */
int model_parser_parse_metadata(model_parser_t parser, const char* model_path, model_metadata_t* metadata);

/**
 * @brief 解析模型输入规格
 * 
 * @param parser 解析器实例
 * @param model_path 模型文件路径
 * @param input_specs 输出的输入规格数组
 * @param input_count 输入数量
 * @return int 0成功，其他失败
 */
int model_parser_parse_inputs(model_parser_t parser, const char* model_path, 
                             model_io_spec_t** input_specs, uint32_t* input_count);

/**
 * @brief 解析模型输出规格
 * 
 * @param parser 解析器实例
 * @param model_path 模型文件路径
 * @param output_specs 输出的输出规格数组
 * @param output_count 输出数量
 * @return int 0成功，其他失败
 */
int model_parser_parse_outputs(model_parser_t parser, const char* model_path, 
                              model_io_spec_t** output_specs, uint32_t* output_count);

/**
 * @brief 验证模型兼容性
 * 
 * @param parser 解析器实例
 * @param model_path 模型文件路径
 * @param backend 目标后端
 * @return bool true兼容，false不兼容
 */
bool model_parser_validate_compatibility(model_parser_t parser, const char* model_path, 
                                        InferBackendType backend);

/**
 * @brief 优化模型配置建议
 * 
 * @param parser 解析器实例
 * @param metadata 模型元信息
 * @param backend 目标后端
 * @param config 输出的优化配置
 * @return int 0成功，其他失败
 */
int model_parser_suggest_config(model_parser_t parser, const model_metadata_t* metadata, 
                               InferBackendType backend, InferEngineConfig* config);

/**
 * @brief 释放模型元信息
 * 
 * @param metadata 元信息结构
 */
void model_metadata_free(model_metadata_t* metadata);

/**
 * @brief 释放IO规格数组
 * 
 * @param specs 规格数组
 * @param count 数量
 */
void model_io_specs_free(model_io_spec_t* specs, uint32_t count);

/**
 * @brief 获取模型格式名称
 * 
 * @param format 格式枚举
 * @return const char* 格式名称
 */
const char* model_format_to_string(model_format_e format);

/**
 * @brief 从字符串解析模型格式
 * 
 * @param format_str 格式字符串
 * @return model_format_e 格式枚举
 */
model_format_e model_format_from_string(const char* format_str);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_MODEL_PARSER_H 