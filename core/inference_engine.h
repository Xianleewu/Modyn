#ifndef MODYN_CORE_INFERENCE_ENGINE_H
#define MODYN_CORE_INFERENCE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明插件工厂类型
typedef struct PluginFactory* plugin_factory_t;

/**
 * @brief 推理后端类型
 */
typedef enum {
    INFER_BACKEND_UNKNOWN = 0,
    INFER_BACKEND_RKNN,         /**< RKNN 后端 */
    INFER_BACKEND_OPENVINO,     /**< OpenVINO 后端 */
    INFER_BACKEND_TENSORRT,     /**< TensorRT 后端 */
    INFER_BACKEND_ONNX,         /**< ONNX Runtime 后端 */
    INFER_BACKEND_DUMMY         /**< 虚拟后端（调试用） */
} infer_backend_type_e;

/**
 * @brief 推理引擎句柄
 */
typedef struct InferEngine* infer_engine_t;

/**
 * @brief 推理引擎配置
 */
typedef struct {
    infer_backend_type_e backend; /**< 后端类型 */
    uint32_t device_id;         /**< 设备ID */
    uint32_t num_threads;       /**< 线程数 */
    bool enable_fp16;           /**< 是否启用FP16 */
    bool enable_int8;           /**< 是否启用INT8 */
    void* custom_config;        /**< 自定义配置 */
} infer_engine_config_t;

/**
 * @brief 推理引擎操作接口
 */
typedef struct {
    /**
     * @brief 创建推理引擎
     */
    infer_engine_t (*create)(const infer_engine_config_t* config);
    
    /**
     * @brief 销毁推理引擎
     */
    void (*destroy)(infer_engine_t engine);
    
    /**
     * @brief 加载模型
     */
    int (*load_model)(infer_engine_t engine, const char* model_path, const void* model_data, size_t model_size);
    
    /**
     * @brief 卸载模型
     */
    int (*unload_model)(infer_engine_t engine);
    
    /**
     * @brief 获取输入张量信息
     */
    int (*get_input_info)(infer_engine_t engine, uint32_t index, Tensor* tensor_info);
    
    /**
     * @brief 获取输出张量信息
     */
    int (*get_output_info)(infer_engine_t engine, uint32_t index, Tensor* tensor_info);
    
    /**
     * @brief 执行推理
     */
    int (*infer)(infer_engine_t engine, const Tensor* inputs, uint32_t input_count, 
                 Tensor* outputs, uint32_t output_count);
    
    /**
     * @brief 获取输入数量
     */
    uint32_t (*get_input_count)(infer_engine_t engine);
    
    /**
     * @brief 获取输出数量
     */
    uint32_t (*get_output_count)(infer_engine_t engine);
    
    /**
     * @brief 获取后端类型
     */
    infer_backend_type_e (*get_backend_type)(infer_engine_t engine);
    
    /**
     * @brief 获取版本信息
     */
    const char* (*get_version)(infer_engine_t engine);
    
} infer_engine_ops_t;

/**
 * @brief 推理引擎工厂
 */
typedef struct {
    infer_backend_type_e backend; /**< 后端类型 */
    const char* name;           /**< 后端名称 */
    const infer_engine_ops_t* ops; /**< 操作接口 */
} infer_engine_factory_t;

// 为了向后兼容，保留旧的类型别名
typedef infer_backend_type_e InferBackendType;
typedef infer_engine_t InferEngine;
typedef infer_engine_config_t InferEngineConfig;
typedef infer_engine_ops_t InferEngineOps;
typedef infer_engine_factory_t InferEngineFactory;

/**
 * @brief 注册推理引擎工厂
 * 
 * @param factory 工厂结构指针
 * @return int 0表示成功，负数表示失败
 */
int infer_engine_register_factory(const InferEngineFactory* factory);

/**
 * @brief 创建推理引擎
 * 
 * @param backend 后端类型
 * @param config 配置
 * @return InferEngine 推理引擎句柄，失败返回NULL
 */
InferEngine infer_engine_create(InferBackendType backend, const InferEngineConfig* config);

/**
 * @brief 销毁推理引擎
 * 
 * @param engine 推理引擎句柄
 */
void infer_engine_destroy(InferEngine engine);

/**
 * @brief 获取可用的后端列表
 * 
 * @param backends 输出的后端类型数组
 * @param count 输入输出参数，输入为数组大小，输出为实际数量
 * @return int 0表示成功，负数表示失败
 */
int infer_engine_get_available_backends(InferBackendType* backends, uint32_t* count);

/**
 * @brief 获取后端名称
 * 
 * @param backend 后端类型
 * @return const char* 后端名称
 */
const char* infer_engine_get_backend_name(InferBackendType backend);

/**
 * @brief 自动检测模型后端类型
 * 
 * @param model_path 模型路径
 * @return InferBackendType 检测到的后端类型
 */
InferBackendType infer_engine_detect_backend(const char* model_path);

/**
 * @brief 加载模型到推理引擎
 * 
 * @param engine 推理引擎
 * @param model_path 模型文件路径
 * @param model_data 模型数据（可选）
 * @param model_size 模型数据大小
 * @return int 0成功，其他失败
 */
int infer_engine_load_model(InferEngine engine, const char* model_path, const void* model_data, size_t model_size);

/**
 * @brief 卸载模型
 * 
 * @param engine 推理引擎
 * @return int 0成功，其他失败
 */
int infer_engine_unload_model(InferEngine engine);

/**
 * @brief 执行推理
 * 
 * @param engine 推理引擎
 * @param inputs 输入张量数组
 * @param input_count 输入张量数量
 * @param outputs 输出张量数组
 * @param output_count 输出张量数量
 * @return int 0成功，其他失败
 */
int infer_engine_infer(InferEngine engine, const Tensor* inputs, uint32_t input_count,
                       Tensor* outputs, uint32_t output_count);

/**
 * @brief 从推理引擎获取后端类型
 * 
 * @param engine 推理引擎
 * @return InferBackendType 后端类型
 */
InferBackendType infer_engine_get_backend_type_from_engine(InferEngine engine);

/* ==================== 插件系统集成 API ==================== */

/**
 * @brief 从指定路径加载推理引擎插件
 * 
 * @param plugin_path 插件文件路径
 * @return int 0成功，其他失败
 */
int infer_engine_load_plugin(const char* plugin_path);

/**
 * @brief 注册插件搜索路径
 * 
 * @param plugin_search_path 插件搜索路径
 * @return int 0成功，其他失败
 */
int infer_engine_register_plugin_path(const char* plugin_search_path);

/**
 * @brief 发现并自动加载所有可用的推理引擎插件
 * 
 * @return int 发现的插件数量，负数表示失败
 */
int infer_engine_discover_plugins(void);

/**
 * @brief 获取全局插件工厂实例
 * 
 * @return plugin_factory_t 插件工厂实例
 */
plugin_factory_t infer_engine_get_plugin_factory(void);

/**
 * @brief 从插件动态创建推理引擎
 * 
 * 如果指定的后端类型未注册，系统会尝试从插件中查找并动态加载
 * 
 * @param backend 后端类型
 * @param config 配置
 * @return InferEngine 推理引擎句柄，失败返回NULL
 */
static inline InferEngine infer_engine_create_from_plugin(InferBackendType backend, const InferEngineConfig* config) {
    return infer_engine_create(backend, config); // 内部会自动尝试从插件加载
}

/**
 * @brief 检查指定后端是否可用（包括插件）
 * 
 * @param backend 后端类型
 * @return bool true表示可用，false表示不可用
 */
bool infer_engine_is_backend_available(InferBackendType backend);

/**
 * @brief 获取所有可用后端的详细信息
 * 
 * @param backend_info 输出的后端信息数组
 * @param count 输入输出参数，输入为数组大小，输出为实际数量
 * @return int 0成功，其他失败
 */
typedef struct {
    InferBackendType backend;
    const char* name;
    const char* version;
    bool is_from_plugin;
    const char* plugin_path;
} backend_info_t;

int infer_engine_get_backend_info_list(backend_info_t* backend_info, uint32_t* count);

/**
 * @brief 热重载指定的推理引擎插件
 * 
 * @param plugin_name 插件名称
 * @return int 0成功，其他失败
 */
int infer_engine_reload_plugin(const char* plugin_name);

/**
 * @brief 卸载指定的推理引擎插件
 * 
 * @param plugin_name 插件名称
 * @return int 0成功，其他失败
 */
int infer_engine_unload_plugin(const char* plugin_name);

/**
 * @brief 列出所有已加载的推理引擎插件
 * 
 * @param plugin_names 输出的插件名称数组
 * @param count 输入输出参数，输入为数组大小，输出为实际数量
 * @return int 0成功，其他失败
 */
int infer_engine_list_plugins(char*** plugin_names, uint32_t* count);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_INFERENCE_ENGINE_H 