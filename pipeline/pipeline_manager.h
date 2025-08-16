#ifndef MODYN_PIPELINE_PIPELINE_MANAGER_H
#define MODYN_PIPELINE_PIPELINE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "core/tensor.h"
#include "core/model_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 管道节点类型
 */
typedef enum {
    PIPELINE_NODE_MODEL = 0,    /**< 模型推理节点 */
    PIPELINE_NODE_PREPROCESS,   /**< 预处理节点 */
    PIPELINE_NODE_POSTPROCESS,  /**< 后处理节点 */
    PIPELINE_NODE_TRANSFORM,    /**< 数据转换节点 */
    PIPELINE_NODE_CUSTOM        /**< 自定义节点 */
} pipeline_node_type_e;

/**
 * @brief 管道节点句柄
 */
typedef struct PipelineNode* pipeline_node_t;

/**
 * @brief 管道句柄
 */
typedef struct Pipeline* pipeline_t;

/**
 * @brief 管道管理器句柄
 */
typedef struct PipelineManager* pipeline_manager_t;

/**
 * @brief 节点处理函数类型
 */
typedef int (*node_process_func_t)(const Tensor* inputs, uint32_t input_count,
                                   Tensor* outputs, uint32_t output_count,
                                   void* context);

/**
 * @brief 管道节点配置
 */
typedef struct {
    char* node_id;              /**< 节点ID */
    pipeline_node_type_e type;  /**< 节点类型 */
    ModelHandle model;          /**< 模型句柄（仅对模型节点有效） */
    node_process_func_t process_func; /**< 处理函数（仅对自定义节点有效） */
    void* context;              /**< 节点上下文数据 */
    uint32_t input_count;       /**< 输入数量 */
    uint32_t output_count;      /**< 输出数量 */
} pipeline_node_config_t;

/**
 * @brief 管道执行模式
 */
typedef enum {
    PIPELINE_EXEC_SEQUENTIAL = 0, /**< 顺序执行 */
    PIPELINE_EXEC_PARALLEL,       /**< 并行执行 */
    PIPELINE_EXEC_PIPELINE        /**< 流水线执行 */
} pipeline_exec_mode_e;

/**
 * @brief 管道配置
 */
typedef struct {
    char* pipeline_id;          /**< 管道ID */
    pipeline_exec_mode_e exec_mode; /**< 执行模式 */
    uint32_t max_parallel;      /**< 最大并行数 */
    bool enable_cache;          /**< 是否启用缓存 */
} pipeline_config_t;

/**
 * @brief 管道连接信息
 */
typedef struct {
    char* from_node;            /**< 源节点ID */
    uint32_t from_output;       /**< 源输出索引 */
    char* to_node;              /**< 目标节点ID */
    uint32_t to_input;          /**< 目标输入索引 */
} pipeline_connection_t;

// 为了向后兼容，保留旧的类型别名
typedef pipeline_node_type_e PipelineNodeType;
typedef pipeline_node_t PipelineNode;
typedef pipeline_t Pipeline;
typedef pipeline_manager_t PipelineManager;
typedef node_process_func_t NodeProcessFunc;
typedef pipeline_node_config_t PipelineNodeConfig;
typedef pipeline_exec_mode_e PipelineExecMode;
typedef pipeline_config_t PipelineConfig;
typedef pipeline_connection_t PipelineConnection;

/**
 * @brief 创建管道管理器
 * 
 * @return PipelineManager 管道管理器句柄，失败返回NULL
 */
PipelineManager pipeline_manager_create(void);

/**
 * @brief 销毁管道管理器
 * 
 * @param manager 管道管理器句柄
 */
void pipeline_manager_destroy(PipelineManager manager);

/**
 * @brief 创建管道
 * 
 * @param manager 管道管理器句柄
 * @param config 管道配置
 * @return Pipeline 管道句柄，失败返回NULL
 */
Pipeline pipeline_create(PipelineManager manager, const PipelineConfig* config);

/**
 * @brief 销毁管道
 * 
 * @param pipeline 管道句柄
 */
void pipeline_destroy(Pipeline pipeline);

/**
 * @brief 添加节点到管道
 * 
 * @param pipeline 管道句柄
 * @param config 节点配置
 * @return PipelineNode 节点句柄，失败返回NULL
 */
PipelineNode pipeline_add_node(Pipeline pipeline, const PipelineNodeConfig* config);

/**
 * @brief 连接两个节点
 * 
 * @param pipeline 管道句柄
 * @param connection 连接信息
 * @return int 0表示成功，负数表示失败
 */
int pipeline_connect_nodes(Pipeline pipeline, const PipelineConnection* connection);

/**
 * @brief 设置管道输入
 * 
 * @param pipeline 管道句柄
 * @param node_id 节点ID
 * @param input_index 输入索引
 * @return int 0表示成功，负数表示失败
 */
int pipeline_set_input(Pipeline pipeline, const char* node_id, uint32_t input_index);

/**
 * @brief 设置管道输出
 * 
 * @param pipeline 管道句柄
 * @param node_id 节点ID
 * @param output_index 输出索引
 * @return int 0表示成功，负数表示失败
 */
int pipeline_set_output(Pipeline pipeline, const char* node_id, uint32_t output_index);

/**
 * @brief 执行管道
 * 
 * @param pipeline 管道句柄
 * @param inputs 输入张量数组
 * @param input_count 输入数量
 * @param outputs 输出张量数组
 * @param output_count 输出数量
 * @return int 0表示成功，负数表示失败
 */
int pipeline_execute(Pipeline pipeline, const Tensor* inputs, uint32_t input_count,
                     Tensor* outputs, uint32_t output_count);

/**
 * @brief 获取管道信息
 * 
 * @param pipeline 管道句柄
 * @param node_count 输出节点数量
 * @param connection_count 输出连接数量
 * @return int 0表示成功，负数表示失败
 */
int pipeline_get_info(Pipeline pipeline, uint32_t* node_count, uint32_t* connection_count);

/**
 * @brief 验证管道拓扑结构
 * 
 * @param pipeline 管道句柄
 * @return int 0表示有效，负数表示无效
 */
int pipeline_validate(Pipeline pipeline);

#ifdef __cplusplus
}
#endif

#endif // MODYN_PIPELINE_PIPELINE_MANAGER_H 