#ifndef MODYN_PIPELINE_H
#define MODYN_PIPELINE_H

#include "../modyn.h"

#ifdef __cplusplus
extern "C" {
#endif

// ======================== Pipeline Core Types ======================== //

/**
 * @brief Pipeline node types
 */
typedef enum __modyn_pipeline_node_type {
  MODYN_PIPELINE_NODE_MODEL = 0,      // 模型推理节点
  MODYN_PIPELINE_NODE_PREPROCESS,     // 前处理节点
  MODYN_PIPELINE_NODE_POSTPROCESS,    // 后处理节点
  MODYN_PIPELINE_NODE_CONDITIONAL,    // 条件分支节点
  MODYN_PIPELINE_NODE_LOOP,           // 循环节点
  MODYN_PIPELINE_NODE_MERGE,          // 数据合并节点
  MODYN_PIPELINE_NODE_SPLIT,          // 数据分割节点
  MODYN_PIPELINE_NODE_CUSTOM,         // 用户自定义节点
  MODYN_PIPELINE_NODE_COUNT
} modyn_pipeline_node_type_e;

/**
 * @brief Pipeline node execution status
 */
typedef enum __modyn_pipeline_node_status {
  MODYN_PIPELINE_NODE_SUCCESS = 0,    // 执行成功
  MODYN_PIPELINE_NODE_ERROR,          // 执行错误
  MODYN_PIPELINE_NODE_SKIP,           // 跳过执行
  MODYN_PIPELINE_NODE_RETRY,          // 需要重试
  MODYN_PIPELINE_NODE_WAIT            // 等待条件满足
} modyn_pipeline_node_status_e;

/**
 * @brief Pipeline node configuration
 */
typedef struct __modyn_pipeline_node_config {
  char name[64];                       // 节点名称
  modyn_pipeline_node_type_e type;    // 节点类型
  void *config_data;                   // 类型特定的配置数据
  size_t config_size;                  // 配置数据大小
  int enabled;                         // 是否启用
  int timeout_ms;                      // 执行超时时间（毫秒）
  int retry_count;                     // 重试次数
  int priority;                        // 执行优先级（数字越小优先级越高）
} modyn_pipeline_node_config_t;

/**
 * @brief Pipeline execution context
 */
typedef struct __modyn_pipeline_exec_context {
  modyn_pipeline_handle_t pipeline;   // Pipeline句柄
  int node_index;                     // 当前执行的节点索引
  void *user_data;                    // 用户数据
  int iteration;                      // 当前迭代次数
  int max_iterations;                 // 最大迭代次数
} modyn_pipeline_exec_context_t;

/**
 * @brief Base pipeline node structure - all node types inherit from this
 */
typedef struct __modyn_pipeline_node {
  // 基本信息
  char name[64];                       // 节点名称
  modyn_pipeline_node_type_e type;    // 节点类型
  modyn_pipeline_node_config_t config; // 节点配置
  
  // 执行接口 - 所有节点类型都必须实现这些方法
  modyn_pipeline_node_status_e (*execute)(struct __modyn_pipeline_node *node,
                                         const modyn_tensor_data_t *inputs,
                                         size_t num_inputs,
                                         modyn_tensor_data_t **outputs,
                                         size_t *num_outputs,
                                         const modyn_pipeline_exec_context_t *context);
  
  modyn_status_e (*validate)(struct __modyn_pipeline_node *node,
                            const modyn_tensor_data_t *inputs,
                            size_t num_inputs);
  
  void (*cleanup)(struct __modyn_pipeline_node *node,
                  modyn_tensor_data_t *outputs,
                  size_t num_outputs);
  
  // 执行期缓存
  modyn_tensor_data_t *cached_outputs;
  size_t cached_num_outputs;
  
  // 执行统计
  int execution_count;
  int success_count;
  int error_count;
  int total_time_ms;
  int last_execution_time_ms;
  
  // 私有数据 - 每种节点类型可以存储自己的特定数据
  void *private_data;
  
  // 析构函数 - 用于清理私有数据
  void (*destroy)(struct __modyn_pipeline_node *node);
} modyn_pipeline_node_t;

/**
 * @brief Unified pipeline node creation function
 * All node types are created through this single interface
 */
typedef modyn_pipeline_node_t* (*modyn_pipeline_node_create_fn)(
  const char *name,
  const void *config_data,
  size_t config_size
);

/**
 * @brief Pipeline node factory - maps node types to creation functions
 */
typedef struct __modyn_pipeline_node_factory {
  modyn_pipeline_node_type_e node_type;
  const char *type_name;
  modyn_pipeline_node_create_fn create_func;
  struct __modyn_pipeline_node_factory *next;
} modyn_pipeline_node_factory_t;

// ======================== Node Configuration Types ======================== //

/**
 * @brief Preprocessing node configuration
 */
typedef struct __modyn_preprocess_config {
  modyn_data_modality_e input_modality;       // 输入数据类型
  modyn_data_modality_e output_modality;      // 输出数据类型
  int resize_enabled;                          // 是否启用尺寸调整
  int resize_width;                            // 目标宽度
  int resize_height;                           // 目标高度
  int normalize_enabled;                       // 是否启用归一化
  float normalize_mean[4];                     // 归一化均值
  float normalize_std[4];                      // 归一化标准差
  int color_space_convert;                     // 颜色空间转换（0=无转换）
} modyn_preprocess_config_t;

/**
 * @brief Postprocessing node configuration
 */
typedef struct __modyn_postprocess_config {
  modyn_data_modality_e input_modality;       // 输入数据类型
  modyn_data_modality_e output_modality;      // 输出数据类型
  int confidence_threshold;                    // 置信度阈值
  int nms_enabled;                            // 是否启用NMS
  float nms_threshold;                        // NMS阈值
  int max_detections;                         // 最大检测数量
  int format_output;                           // 输出格式（0=原始，1=JSON，2=XML）
} modyn_postprocess_config_t;

/**
 * @brief Conditional node configuration
 */
typedef struct __modyn_conditional_config {
  int condition_type;                          // 条件类型（0=阈值，1=表达式，2=自定义函数）
  float threshold;                             // 阈值（用于阈值类型）
  char expression[256];                        // 表达式字符串
  void *condition_func;                        // 自定义条件函数
  int true_branch;                             // 条件为真时的分支节点索引
  int false_branch;                            // 条件为假时的分支节点索引
} modyn_conditional_config_t;

/**
 * @brief Loop node configuration
 */
typedef struct __modyn_loop_config {
  int loop_type;                               // 循环类型（0=固定次数，1=条件循环，2=无限循环）
  int max_iterations;                          // 最大迭代次数
  char condition_expr[256];                    // 循环条件表达式
  int break_on_error;                          // 错误时是否跳出循环
  int continue_on_skip;                        // 跳过时是否继续循环
} modyn_loop_config_t;

/**
 * @brief Custom node configuration
 */
typedef struct __modyn_custom_config {
  char library_path[256];                      // 动态库路径
  char function_name[64];                      // 函数名称
  void *user_data;                             // 用户自定义数据
  size_t user_data_size;                       // 用户数据大小
} modyn_custom_config_t;

// ======================== Pipeline Core API ======================== //

/**
 * @brief Create a new pipeline
 *
 * @param name Pipeline name
 * @return modyn_pipeline_handle_t Pipeline handle
 */
modyn_pipeline_handle_t modyn_create_pipeline(const char *name);

/**
 * @brief Add a node to pipeline using unified interface
 * This is the single entry point for adding any type of node
 *
 * @param pipeline Pipeline handle
 * @param node_type Type of node to add
 * @param node_name Node name
 * @param config_data Node configuration data
 * @param config_size Size of configuration data
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_pipeline_add_node_by_type(modyn_pipeline_handle_t pipeline,
                                              modyn_pipeline_node_type_e node_type,
                                              const char *node_name,
                                              const void *config_data,
                                              size_t config_size);

/**
 * @brief Add a model node to pipeline (backward compatibility)
 *
 * @param pipeline Pipeline handle
 * @param model_handle Model handle
 * @param node_name Node name
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_pipeline_add_node(modyn_pipeline_handle_t pipeline,
                                      modyn_model_handle_t model_handle,
                                      const char *node_name);

/**
 * @brief Connect nodes in pipeline
 *
 * @param pipeline Pipeline handle
 * @param src_node Source node name
 * @param src_output_idx Source output index
 * @param dst_node Destination node name
 * @param dst_input_idx Destination input index
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_pipeline_connect_nodes(modyn_pipeline_handle_t pipeline,
                                           const char *src_node, size_t src_output_idx,
                                           const char *dst_node, size_t dst_input_idx);

/**
 * @brief Run pipeline with input data
 *
 * @param pipeline Pipeline handle
 * @param inputs Input tensor data
 * @param num_inputs Number of input tensors
 * @param outputs [out] Output tensor data
 * @param num_outputs [out] Number of output tensors
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_run_pipeline(modyn_pipeline_handle_t pipeline,
                                 modyn_tensor_data_t *inputs, size_t num_inputs,
                                 modyn_tensor_data_t **outputs, size_t *num_outputs);

// ======================== Node Type Management ======================== //

/**
 * @brief Register a node type factory
 * This allows users to register custom node types
 *
 * @param node_type Node type identifier
 * @param type_name Human-readable type name
 * @param create_func Function to create nodes of this type
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_register_pipeline_node_type(modyn_pipeline_node_type_e node_type,
                                                const char *type_name,
                                                modyn_pipeline_node_create_fn create_func);

/**
 * @brief Unregister a node type factory
 *
 * @param node_type Node type identifier
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_unregister_pipeline_node_type(modyn_pipeline_node_type_e node_type);

// ======================== Pipeline Control & Monitoring ======================== //

/**
 * @brief Set pipeline execution options
 *
 * @param pipeline Pipeline handle
 * @param timeout_ms Execution timeout in milliseconds
 * @param max_retries Maximum retry count
 * @param enable_parallel Enable parallel execution for independent nodes
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_pipeline_set_execution_options(modyn_pipeline_handle_t pipeline,
                                                   int timeout_ms,
                                                   int max_retries,
                                                   int enable_parallel);

/**
 * @brief Get pipeline execution statistics
 *
 * @param pipeline Pipeline handle
 * @param total_nodes Total number of nodes
 * @param executed_nodes Number of executed nodes
 * @param skipped_nodes Number of skipped nodes
 * @param error_nodes Number of error nodes
 * @param total_time_ms Total execution time in milliseconds
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_pipeline_get_execution_stats(modyn_pipeline_handle_t pipeline,
                                                 size_t *total_nodes,
                                                 size_t *executed_nodes,
                                                 size_t *skipped_nodes,
                                                 size_t *error_nodes,
                                                 int *total_time_ms);

// ======================== Pipeline Topology ======================== //

/**
 * @brief Pipeline node information for topology query
 */
typedef struct __modyn_pipeline_node_info {
  char name[64];                              // 节点名称
  modyn_model_handle_t model_handle;          // 关联的模型句柄
  size_t num_inputs;                          // 输入数量
  size_t num_outputs;                         // 输出数量
  int is_source;                              // 是否为源节点（无输入边）
  int is_sink;                                // 是否为汇节点（无输出边）
} modyn_pipeline_node_info_t;

/**
 * @brief Pipeline edge information for topology query
 */
typedef struct __modyn_pipeline_edge_info {
  char src_node[64];                          // 源节点名称
  size_t src_output_idx;                      // 源节点输出索引
  size_t dst_input_idx;                       // 目标节点输入索引
  char dst_node[64];                          // 目标节点名称
} modyn_pipeline_edge_info_t;

/**
 * @brief Pipeline topology information
 */
typedef struct __modyn_pipeline_topology {
  modyn_pipeline_node_info_t *nodes;          // 节点信息数组
  size_t num_nodes;                           // 节点数量
  modyn_pipeline_edge_info_t *edges;          // 边信息数组
  size_t num_edges;                           // 边数量
  char name[64];                              // Pipeline名称
} modyn_pipeline_topology_t;

/**
 * @brief Query pipeline topology structure
 *
 * @param pipeline Pipeline handle
 * @param topology [out] Pipeline topology information
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_query_pipeline_topology(modyn_pipeline_handle_t pipeline,
                                            modyn_pipeline_topology_t *topology);

/**
 * @brief Free pipeline topology information
 *
 * @param topology Topology information to free
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_free_pipeline_topology(modyn_pipeline_topology_t *topology);

// ======================== Node Management & Discovery ======================== //

/**
 * @brief Node search criteria for finding specific nodes
 */
typedef struct __modyn_node_search_criteria {
  modyn_pipeline_node_type_e node_type;    // 节点类型（可选，-1表示任意类型）
  const char *name_pattern;                // 名称模式（支持通配符，NULL表示任意名称）
  const char *tag;                         // 标签（NULL表示任意标签）
  int min_priority;                        // 最小优先级
  int max_priority;                        // 最大优先级
  int enabled_only;                        // 是否只搜索启用的节点
} modyn_node_search_criteria_t;

/**
 * @brief Node information for discovery
 */
typedef struct __modyn_node_info {
  char name[64];                           // 节点名称
  modyn_pipeline_node_type_e type;         // 节点类型
  const char *type_name;                   // 类型名称
  char tag[32];                            // 节点标签
  int priority;                            // 优先级
  int enabled;                             // 是否启用
  int execution_count;                     // 执行次数
  int success_count;                       // 成功次数
  int error_count;                         // 错误次数
  int total_time_ms;                       // 总执行时间
  void *private_data;                      // 私有数据
} modyn_node_info_t;

/**
 * @brief Find nodes matching search criteria
 *
 * @param criteria Search criteria
 * @param nodes [out] Array of matching nodes
 * @param max_nodes Maximum nodes to return
 * @param num_found [out] Number of nodes found
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_find_nodes(const modyn_node_search_criteria_t *criteria,
                                modyn_node_info_t *nodes,
                                size_t max_nodes,
                                size_t *num_found);

/**
 * @brief Get node by name
 *
 * @param node_name Node name
 * @param node_info [out] Node information
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_node_by_name(const char *node_name,
                                     modyn_node_info_t *node_info);

/**
 * @brief Get all nodes of a specific type
 *
 * @param node_type Node type
 * @param nodes [out] Array of nodes
 * @param max_nodes Maximum nodes to return
 * @param num_found [out] Number of nodes found
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_nodes_by_type(modyn_pipeline_node_type_e node_type,
                                      modyn_node_info_t *nodes,
                                      size_t max_nodes,
                                      size_t *num_found);

/**
 * @brief Tag a node for easier discovery
 *
 * @param node_name Node name
 * @param tag Tag string
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_tag_node(const char *node_name, const char *tag);

/**
 * @brief Remove tag from a node
 *
 * @param node_name Node name
 * @param tag Tag string
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_untag_node(const char *node_name, const char *tag);

/**
 * @brief Get node statistics
 *
 * @param node_name Node name
 * @param stats [out] Node statistics
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_node_stats(const char *node_name,
                                   modyn_node_info_t *stats);

#ifdef __cplusplus
}
#endif

#endif // MODYN_PIPELINE_H
