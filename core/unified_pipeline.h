#ifndef MODYN_CORE_UNIFIED_PIPELINE_H
#define MODYN_CORE_UNIFIED_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 统一推理流水线系统
 * 
 * 设计原则：
 * 1. 所有处理单元的输入输出都是tensor
 * 2. 不区分模型推理、预处理、后处理等，都是统一的处理单元
 * 3. 处理单元可以随意组合，形成复杂的处理流水线
 * 4. 支持zero-copy和内存池优化
 */

// 前向声明
typedef struct unified_pipeline_s unified_pipeline_t;
typedef struct processing_unit_s processing_unit_t;
typedef struct tensor_map_s tensor_map_t;

/**
 * @brief 处理单元函数签名
 * 
 * @param inputs 输入tensor映射表
 * @param outputs 输出tensor映射表  
 * @param context 用户上下文
 * @return int 0成功，负数失败
 */
typedef int (*process_func_t)(const tensor_map_t* inputs, tensor_map_t* outputs, void* context);

/**
 * @brief 处理单元类型
 */
typedef enum {
    UNIT_TYPE_FUNCTION = 0,     /**< 函数处理单元 */
    UNIT_TYPE_MODEL,            /**< 模型推理单元 */
    UNIT_TYPE_PARALLEL,         /**< 并行处理单元 */
    UNIT_TYPE_CONDITIONAL,      /**< 条件分支单元 */
    UNIT_TYPE_LOOP             /**< 循环处理单元 */
} unit_type_e;

/**
 * @brief 处理单元配置
 */
typedef struct {
    char name[64];              /**< 单元名称 */
    unit_type_e type;           /**< 单元类型 */
    process_func_t process;     /**< 处理函数 */
    void* config;               /**< 配置参数 */
    void* context;              /**< 用户上下文 */
    bool async_mode;            /**< 是否异步执行 */
    uint32_t timeout_ms;        /**< 超时时间(毫秒) */
} unit_config_t;

/**
 * @brief Tensor映射表 - 用于在处理单元间传递多个tensor
 */
struct tensor_map_s {
    char** keys;                /**< tensor名称数组 */
    tensor_t** tensors;         /**< tensor指针数组 */
    uint32_t count;             /**< tensor数量 */
    uint32_t capacity;          /**< 容量 */
};

/**
 * @brief 处理单元
 */
struct processing_unit_s {
    char name[64];              /**< 单元名称 */
    unit_type_e type;           /**< 单元类型 */
    process_func_t process;     /**< 处理函数 */
    void* config;               /**< 配置参数 */
    void* context;              /**< 用户上下文 */
    char** input_keys;          /**< 输入tensor名称 */
    char** output_keys;         /**< 输出tensor名称 */
    uint32_t input_count;       /**< 输入数量 */
    uint32_t output_count;      /**< 输出数量 */
    bool async_mode;            /**< 异步模式 */
    uint32_t timeout_ms;        /**< 超时时间 */
};

/**
 * @brief 统一流水线
 */
struct unified_pipeline_s {
    char name[128];             /**< 流水线名称 */
    processing_unit_t* units;   /**< 处理单元数组 */
    uint32_t unit_count;        /**< 单元数量 */
    uint32_t capacity;          /**< 容量 */
    tensor_map_t* global_map;   /**< 全局tensor映射表 */
    bool enable_memory_pool;    /**< 是否启用内存池 */
    void* memory_pool;          /**< 内存池 */
    bool debug_mode;            /**< 调试模式 */
};

/**
 * @brief 执行完成回调
 */
typedef void (*pipeline_completion_callback_t)(unified_pipeline_t* pipeline, int result, void* user_data);

/**
 * @brief 步骤执行回调
 */
typedef void (*unit_execution_callback_t)(const char* unit_name, int result, double execution_time_ms);

// ================================
// Tensor Map API
// ================================

/**
 * @brief 创建tensor映射表
 * 
 * @param initial_capacity 初始容量
 * @return tensor_map_t* 创建的映射表
 */
tensor_map_t* tensor_map_create(uint32_t initial_capacity);

/**
 * @brief 销毁tensor映射表
 * 
 * @param map 映射表
 */
void tensor_map_destroy(tensor_map_t* map);

/**
 * @brief 添加tensor到映射表
 * 
 * @param map 映射表
 * @param key tensor名称
 * @param tensor tensor指针
 * @return int 0成功，负数失败
 */
int tensor_map_set(tensor_map_t* map, const char* key, tensor_t* tensor);

/**
 * @brief 从映射表获取tensor
 * 
 * @param map 映射表
 * @param key tensor名称
 * @return tensor_t* tensor指针，未找到返回NULL
 */
tensor_t* tensor_map_get(const tensor_map_t* map, const char* key);

/**
 * @brief 检查映射表中是否存在指定tensor
 * 
 * @param map 映射表
 * @param key tensor名称
 * @return bool 存在返回true
 */
bool tensor_map_has(const tensor_map_t* map, const char* key);

/**
 * @brief 从映射表移除tensor
 * 
 * @param map 映射表
 * @param key tensor名称
 * @return int 0成功，负数失败
 */
int tensor_map_remove(tensor_map_t* map, const char* key);

/**
 * @brief 获取映射表中tensor数量
 * 
 * @param map 映射表
 * @return uint32_t tensor数量
 */
uint32_t tensor_map_size(const tensor_map_t* map);

/**
 * @brief 清空映射表
 * 
 * @param map 映射表
 */
void tensor_map_clear(tensor_map_t* map);

/**
 * @brief 复制映射表
 * 
 * @param src 源映射表
 * @return tensor_map_t* 复制的映射表
 */
tensor_map_t* tensor_map_copy(const tensor_map_t* src);

// ================================
// Processing Unit API
// ================================

/**
 * @brief 创建函数处理单元
 * 
 * @param name 单元名称
 * @param process_func 处理函数
 * @param context 用户上下文
 * @param input_keys 输入tensor名称数组
 * @param input_count 输入数量
 * @param output_keys 输出tensor名称数组
 * @param output_count 输出数量
 * @return processing_unit_t* 创建的处理单元
 */
processing_unit_t* create_function_unit(const char* name, 
                                       process_func_t process_func,
                                       void* context,
                                       const char** input_keys, uint32_t input_count,
                                       const char** output_keys, uint32_t output_count);

/**
 * @brief 创建模型推理单元
 * 
 * @param name 单元名称
 * @param model_path 模型路径
 * @param input_keys 输入tensor名称数组
 * @param input_count 输入数量
 * @param output_keys 输出tensor名称数组
 * @param output_count 输出数量
 * @return processing_unit_t* 创建的处理单元
 */
processing_unit_t* create_model_unit(const char* name,
                                    const char* model_path,
                                    const char** input_keys, uint32_t input_count,
                                    const char** output_keys, uint32_t output_count);

/**
 * @brief 创建并行处理单元
 * 
 * @param name 单元名称
 * @param sub_units 子处理单元数组
 * @param unit_count 子单元数量
 * @return processing_unit_t* 创建的处理单元
 */
processing_unit_t* create_parallel_unit(const char* name,
                                       processing_unit_t** sub_units,
                                       uint32_t unit_count);

/**
 * @brief 创建条件分支单元
 * 
 * @param name 单元名称
 * @param condition_func 条件判断函数
 * @param true_unit 条件为真时执行的单元
 * @param false_unit 条件为假时执行的单元
 * @return processing_unit_t* 创建的处理单元
 */
processing_unit_t* create_conditional_unit(const char* name,
                                          process_func_t condition_func,
                                          processing_unit_t* true_unit,
                                          processing_unit_t* false_unit);

/**
 * @brief 创建循环处理单元
 * 
 * @param name 单元名称
 * @param loop_condition 循环条件函数
 * @param loop_body 循环体单元
 * @param max_iterations 最大迭代次数
 * @return processing_unit_t* 创建的处理单元
 */
processing_unit_t* create_loop_unit(const char* name,
                                   process_func_t loop_condition,
                                   processing_unit_t* loop_body,
                                   uint32_t max_iterations);

/**
 * @brief 销毁处理单元
 * 
 * @param unit 处理单元
 */
void processing_unit_destroy(processing_unit_t* unit);

/**
 * @brief 执行处理单元
 * 
 * @param unit 处理单元
 * @param inputs 输入tensor映射表
 * @param outputs 输出tensor映射表
 * @return int 0成功，负数失败
 */
int processing_unit_execute(processing_unit_t* unit, 
                           const tensor_map_t* inputs, 
                           tensor_map_t* outputs);

// ================================
// Unified Pipeline API
// ================================

/**
 * @brief 创建统一流水线
 * 
 * @param name 流水线名称
 * @return unified_pipeline_t* 创建的流水线
 */
unified_pipeline_t* unified_pipeline_create(const char* name);

/**
 * @brief 销毁统一流水线
 * 
 * @param pipeline 流水线
 */
void unified_pipeline_destroy(unified_pipeline_t* pipeline);

/**
 * @brief 添加处理单元到流水线
 * 
 * @param pipeline 流水线
 * @param unit 处理单元
 * @return int 0成功，负数失败
 */
int unified_pipeline_add_unit(unified_pipeline_t* pipeline, processing_unit_t* unit);

/**
 * @brief 执行流水线
 * 
 * @param pipeline 流水线
 * @param inputs 输入tensor映射表
 * @param outputs 输出tensor映射表
 * @return int 0成功，负数失败
 */
int unified_pipeline_execute(unified_pipeline_t* pipeline,
                            const tensor_map_t* inputs,
                            tensor_map_t* outputs);

/**
 * @brief 异步执行流水线
 * 
 * @param pipeline 流水线
 * @param inputs 输入tensor映射表
 * @param outputs 输出tensor映射表
 * @param callback 完成回调
 * @param user_data 用户数据
 * @return int 0成功，负数失败
 */
int unified_pipeline_execute_async(unified_pipeline_t* pipeline,
                                  const tensor_map_t* inputs,
                                  tensor_map_t* outputs,
                                  pipeline_completion_callback_t callback,
                                  void* user_data);

/**
 * @brief 设置流水线内存池
 * 
 * @param pipeline 流水线
 * @param memory_pool 内存池
 * @return int 0成功，负数失败
 */
int unified_pipeline_set_memory_pool(unified_pipeline_t* pipeline, void* memory_pool);

/**
 * @brief 设置调试模式
 * 
 * @param pipeline 流水线
 * @param enable 是否启用
 */
void unified_pipeline_set_debug_mode(unified_pipeline_t* pipeline, bool enable);

/**
 * @brief 设置单元执行回调
 * 
 * @param pipeline 流水线
 * @param callback 回调函数
 */
void unified_pipeline_set_unit_callback(unified_pipeline_t* pipeline, unit_execution_callback_t callback);

/**
 * @brief 获取流水线统计信息
 * 
 * @param pipeline 流水线
 * @param total_units 总单元数
 * @param total_execution_time 总执行时间(毫秒)
 * @param avg_unit_time 平均单元执行时间(毫秒)
 * @return int 0成功，负数失败
 */
int unified_pipeline_get_stats(unified_pipeline_t* pipeline,
                              uint32_t* total_units,
                              double* total_execution_time,
                              double* avg_unit_time);

// ================================
// 便利宏和辅助函数
// ================================

/**
 * @brief 便利宏：创建流水线
 */
#define PIPELINE_CREATE(name) \
    unified_pipeline_t* pipeline = unified_pipeline_create(name)

/**
 * @brief 便利宏：添加函数单元
 */
#define PIPELINE_ADD_FUNC(name, func, ctx, inputs, outputs) \
    do { \
        const char* _inputs[] = inputs; \
        const char* _outputs[] = outputs; \
        processing_unit_t* unit = create_function_unit(name, func, ctx, \
            _inputs, sizeof(_inputs)/sizeof(_inputs[0]), \
            _outputs, sizeof(_outputs)/sizeof(_outputs[0])); \
        unified_pipeline_add_unit(pipeline, unit); \
    } while(0)

/**
 * @brief 便利宏：添加模型单元
 */
#define PIPELINE_ADD_MODEL(name, model_path, inputs, outputs) \
    do { \
        const char* _inputs[] = inputs; \
        const char* _outputs[] = outputs; \
        processing_unit_t* unit = create_model_unit(name, model_path, \
            _inputs, sizeof(_inputs)/sizeof(_inputs[0]), \
            _outputs, sizeof(_outputs)/sizeof(_outputs[0])); \
        unified_pipeline_add_unit(pipeline, unit); \
    } while(0)

/**
 * @brief 便利宏：执行流水线
 */
#define PIPELINE_EXECUTE(inputs_map, outputs_map) \
    unified_pipeline_execute(pipeline, inputs_map, outputs_map)

// ================================
// 常用处理函数
// ================================

/**
 * @brief 图像预处理函数：调整大小、归一化等
 * 
 * @param inputs 输入包含"image"张量
 * @param outputs 输出包含"processed_image"张量
 * @param context 包含预处理配置的上下文
 * @return int 0成功，负数失败
 */
int image_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context);

/**
 * @brief 文本预处理函数：分词、编码等
 * 
 * @param inputs 输入包含"text"张量
 * @param outputs 输出包含"tokens"张量
 * @param context 包含分词器配置的上下文
 * @return int 0成功，负数失败
 */
int text_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context);

/**
 * @brief 音频预处理函数：重采样、特征提取等
 * 
 * @param inputs 输入包含"audio"张量
 * @param outputs 输出包含"features"张量
 * @param context 包含音频配置的上下文
 * @return int 0成功，负数失败
 */
int audio_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context);

/**
 * @brief 分类后处理函数：softmax、argmax等
 * 
 * @param inputs 输入包含"logits"张量
 * @param outputs 输出包含"predictions"和"probabilities"张量
 * @param context 包含类别信息的上下文
 * @return int 0成功，负数失败
 */
int classification_postprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context);

/**
 * @brief 音频合成后处理函数：降噪、增强等
 * 
 * @param inputs 输入包含"raw_audio"张量
 * @param outputs 输出包含"enhanced_audio"张量
 * @param context 包含后处理配置的上下文
 * @return int 0成功，负数失败
 */
int audio_postprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_UNIFIED_PIPELINE_H