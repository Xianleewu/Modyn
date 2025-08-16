#ifndef MODYN_CORE_MODEL_MANAGER_H
#define MODYN_CORE_MODEL_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "core/tensor.h"
#include "core/inference_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模型句柄
 */
typedef struct ModelHandle* model_handle_t;

/**
 * @brief 模型管理器
 */
typedef struct ModelManager model_manager_t;

/**
 * @brief 模型配置结构
 */
typedef struct {
    char* model_path;           /**< 模型文件路径 */
    char* model_id;             /**< 模型唯一标识符 */
    char* version;              /**< 模型版本 */
    InferBackendType backend;   /**< 推理后端类型 */
    uint32_t max_instances;     /**< 最大实例数 */
    bool enable_cache;          /**< 是否启用缓存 */
    void* custom_config;        /**< 自定义配置 */
} model_config_t;

/**
 * @brief 模型状态枚举
 */
typedef enum {
    MODEL_STATUS_UNLOADED = 0,  /**< 未加载 */
    MODEL_STATUS_LOADING,       /**< 加载中 */
    MODEL_STATUS_LOADED,        /**< 已加载 */
    MODEL_STATUS_ERROR          /**< 错误状态 */
} model_status_e;

/**
 * @brief 模型信息结构
 */
typedef struct {
    char* model_id;             /**< 模型ID */
    char* version;              /**< 版本 */
    model_status_e status;      /**< 状态 */
    uint32_t instance_count;    /**< 实例数量 */
    uint64_t memory_usage;      /**< 内存使用量 */
    uint64_t inference_count;   /**< 推理次数 */
    double avg_latency;         /**< 平均延迟 */
} model_info_t;

// 为了向后兼容，保留旧的类型别名
typedef model_handle_t ModelHandle;
typedef model_manager_t ModelManager;
typedef model_config_t ModelConfig;
typedef model_status_e ModelStatus;
typedef model_info_t ModelInfo;

/**
 * @brief 创建模型管理器
 * 
 * @return model_manager_t* 模型管理器指针，失败返回NULL
 */
model_manager_t* model_manager_create(void);

/**
 * @brief 销毁模型管理器
 * 
 * @param manager 模型管理器指针
 */
void model_manager_destroy(model_manager_t* manager);

/**
 * @brief 加载模型
 * 
 * @param manager 模型管理器指针
 * @param model_path 模型文件路径
 * @param config 模型配置，可以为NULL使用默认配置
 * @return model_handle_t 模型句柄，失败返回NULL
 */
model_handle_t model_manager_load(model_manager_t* manager, const char* model_path, const model_config_t* config);

/**
 * @brief 卸载模型
 * 
 * @param manager 模型管理器指针
 * @param model 模型句柄
 * @return int 0表示成功，负数表示失败
 */
int model_manager_unload(model_manager_t* manager, model_handle_t model);

/**
 * @brief 获取模型句柄
 * 
 * @param manager 模型管理器指针
 * @param model_id 模型ID
 * @return model_handle_t 模型句柄，未找到返回NULL
 */
model_handle_t model_manager_get(model_manager_t* manager, const char* model_id);

/**
 * @brief 获取模型信息
 * 
 * @param manager 模型管理器指针
 * @param model_id 模型ID
 * @param info 输出的模型信息
 * @return int 0表示成功，负数表示失败
 */
int model_manager_get_info(model_manager_t* manager, const char* model_id, model_info_t* info);

/**
 * @brief 列出所有模型
 * 
 * @param manager 模型管理器指针
 * @param model_ids 输出的模型ID数组
 * @param count 输入输出参数，输入为数组大小，输出为实际数量
 * @return int 0表示成功，负数表示失败
 */
int model_manager_list(model_manager_t* manager, char** model_ids, uint32_t* count);

/**
 * @brief 执行模型推理
 * 
 * @param model 模型句柄
 * @param input_tensors 输入张量数组
 * @param input_count 输入张量数量
 * @param output_tensors 输出张量数组
 * @param output_count 输出张量数量
 * @return int 0表示成功，负数表示失败
 */
int model_infer(model_handle_t model, const tensor_t* input_tensors, uint32_t input_count,
                tensor_t* output_tensors, uint32_t output_count);

/**
 * @brief 简化的单输入单输出推理接口
 * 
 * @param model 模型句柄
 * @param input 输入张量
 * @param output 输出张量
 * @return int 0表示成功，负数表示失败
 */
int model_infer_simple(model_handle_t model, const tensor_t* input, tensor_t* output);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_MODEL_MANAGER_H 