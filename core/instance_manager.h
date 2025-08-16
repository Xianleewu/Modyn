#ifndef MODYN_CORE_INSTANCE_MANAGER_H
#define MODYN_CORE_INSTANCE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "core/inference_engine.h"
#include "core/memory_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 实例状态枚举
 */
typedef enum {
    INSTANCE_STATUS_IDLE = 0,       /**< 空闲状态 */
    INSTANCE_STATUS_BUSY,           /**< 忙碌状态 */
    INSTANCE_STATUS_LOADING,        /**< 加载中 */
    INSTANCE_STATUS_ERROR,          /**< 错误状态 */
    INSTANCE_STATUS_UNLOADED        /**< 已卸载 */
} instance_status_e;

/**
 * @brief 实例共享类型
 */
typedef enum {
    INSTANCE_SHARE_NONE = 0,        /**< 不共享 */
    INSTANCE_SHARE_WEIGHTS,         /**< 权重共享 */
    INSTANCE_SHARE_MEMORY,          /**< 内存共享 */
    INSTANCE_SHARE_FULL             /**< 完全共享 */
} instance_share_type_e;

/**
 * @brief 实例调度策略
 */
typedef enum {
    INSTANCE_SCHED_ROUND_ROBIN = 0, /**< 轮询调度 */
    INSTANCE_SCHED_LEAST_LOADED,    /**< 最少负载 */
    INSTANCE_SCHED_RANDOM,          /**< 随机调度 */
    INSTANCE_SCHED_PRIORITY,        /**< 优先级调度 */
    INSTANCE_SCHED_STICKY           /**< 粘性调度 */
} instance_schedule_strategy_e;

/**
 * @brief 实例信息
 */
typedef struct {
    uint32_t instance_id;           /**< 实例ID */
    char* model_id;                 /**< 模型ID */
    instance_status_e status;       /**< 实例状态 */
    InferEngine engine;             /**< 推理引擎 */
    MemoryHandle shared_weights;    /**< 共享权重 */
    MemoryHandle private_memory;    /**< 私有内存 */
    uint64_t created_time;          /**< 创建时间 */
    uint64_t last_used_time;        /**< 最后使用时间 */
    uint32_t inference_count;       /**< 推理次数 */
    double avg_latency;             /**< 平均延迟 */
    uint32_t priority;              /**< 优先级 */
    pthread_mutex_t mutex;          /**< 实例互斥锁 */
} instance_info_t;

/**
 * @brief 实例池配置
 */
typedef struct {
    char* model_id;                 /**< 模型ID */
    uint32_t min_instances;         /**< 最小实例数 */
    uint32_t max_instances;         /**< 最大实例数 */
    uint32_t idle_timeout;          /**< 空闲超时（秒） */
    instance_share_type_e share_type; /**< 共享类型 */
    instance_schedule_strategy_e schedule_strategy; /**< 调度策略 */
    InferEngineConfig engine_config; /**< 引擎配置 */
    MemoryPool memory_pool;         /**< 内存池 */
    bool enable_preload;            /**< 是否预加载 */
    bool enable_warmup;             /**< 是否预热 */
    uint32_t warmup_iterations;     /**< 预热迭代次数 */
} instance_pool_config_t;

/**
 * @brief 实例池统计信息
 */
typedef struct {
    uint32_t total_instances;       /**< 总实例数 */
    uint32_t active_instances;      /**< 活跃实例数 */
    uint32_t idle_instances;        /**< 空闲实例数 */
    uint32_t busy_instances;        /**< 忙碌实例数 */
    uint32_t error_instances;       /**< 错误实例数 */
    uint64_t total_inferences;      /**< 总推理次数 */
    double avg_latency;             /**< 平均延迟 */
    double avg_throughput;          /**< 平均吞吐量 */
    uint64_t memory_usage;          /**< 内存使用量 */
    uint64_t shared_memory_usage;   /**< 共享内存使用量 */
} instance_pool_stats_t;

/**
 * @brief 实例句柄
 */
typedef struct ModelInstance* model_instance_t;

/**
 * @brief 实例池句柄
 */
typedef struct InstancePool* instance_pool_t;

/**
 * @brief 实例管理器句柄
 */
typedef struct InstanceManager* instance_manager_t;

// 为了向后兼容，保留旧的类型别名
typedef instance_status_e InstanceStatus;
typedef instance_share_type_e InstanceShareType;
typedef instance_schedule_strategy_e InstanceScheduleStrategy;
typedef instance_info_t InstanceInfo;
typedef instance_pool_config_t InstancePoolConfig;
typedef instance_pool_stats_t InstancePoolStats;
typedef model_instance_t ModelInstance;
typedef instance_pool_t InstancePool;
typedef instance_manager_t InstanceManager;

/**
 * @brief 创建实例管理器
 * 
 * @param memory_pool 内存池
 * @return InstanceManager 实例管理器
 */
InstanceManager instance_manager_create(MemoryPool memory_pool);

/**
 * @brief 销毁实例管理器
 * 
 * @param manager 实例管理器
 */
void instance_manager_destroy(InstanceManager manager);

/**
 * @brief 创建实例池
 * 
 * @param manager 实例管理器
 * @param config 实例池配置
 * @return InstancePool 实例池
 */
InstancePool instance_manager_create_pool(InstanceManager manager, const InstancePoolConfig* config);

/**
 * @brief 销毁实例池
 * 
 * @param pool 实例池
 */
void instance_pool_destroy(InstancePool pool);

/**
 * @brief 获取实例
 * 
 * @param pool 实例池
 * @param timeout_ms 超时时间（毫秒）
 * @return ModelInstance 实例句柄
 */
ModelInstance instance_pool_acquire(InstancePool pool, uint32_t timeout_ms);

/**
 * @brief 释放实例
 * 
 * @param pool 实例池
 * @param instance 实例句柄
 * @return int 0成功，其他失败
 */
int instance_pool_release(InstancePool pool, ModelInstance instance);

/**
 * @brief 实例推理
 * 
 * @param instance 实例句柄
 * @param inputs 输入张量
 * @param input_count 输入数量
 * @param outputs 输出张量
 * @param output_count 输出数量
 * @return int 0成功，其他失败
 */
int instance_infer(ModelInstance instance, const Tensor* inputs, uint32_t input_count,
                  Tensor* outputs, uint32_t output_count);

/**
 * @brief 获取实例信息
 * 
 * @param instance 实例句柄
 * @param info 实例信息输出
 * @return int 0成功，其他失败
 */
int instance_get_info(ModelInstance instance, InstanceInfo* info);

/**
 * @brief 获取实例池统计信息
 * 
 * @param pool 实例池
 * @param stats 统计信息输出
 * @return int 0成功，其他失败
 */
int instance_pool_get_stats(InstancePool pool, InstancePoolStats* stats);

/**
 * @brief 设置实例池大小
 * 
 * @param pool 实例池
 * @param min_instances 最小实例数
 * @param max_instances 最大实例数
 * @return int 0成功，其他失败
 */
int instance_pool_resize(InstancePool pool, uint32_t min_instances, uint32_t max_instances);

/**
 * @brief 预热实例池
 * 
 * @param pool 实例池
 * @param iterations 预热迭代次数
 * @return int 0成功，其他失败
 */
int instance_pool_warmup(InstancePool pool, uint32_t iterations);

/**
 * @brief 清理空闲实例
 * 
 * @param pool 实例池
 * @return int 清理的实例数
 */
int instance_pool_cleanup_idle(InstancePool pool);

/**
 * @brief 获取实例池中的所有实例
 * 
 * @param pool 实例池
 * @param instances 实例数组输出
 * @param count 实例数量输出
 * @return int 0成功，其他失败
 */
int instance_pool_get_instances(InstancePool pool, InstanceInfo** instances, uint32_t* count);

/**
 * @brief 设置实例池调度策略
 * 
 * @param pool 实例池
 * @param strategy 调度策略
 * @return int 0成功，其他失败
 */
int instance_pool_set_schedule_strategy(InstancePool pool, InstanceScheduleStrategy strategy);

/**
 * @brief 获取实例池模型ID
 * 
 * @param pool 实例池
 * @return const char* 模型ID
 */
const char* instance_pool_get_model_id(InstancePool pool);

/**
 * @brief 检查实例是否可用
 * 
 * @param instance 实例句柄
 * @return bool true可用，false不可用
 */
bool instance_is_available(ModelInstance instance);

/**
 * @brief 获取实例引擎
 * 
 * @param instance 实例句柄
 * @return InferEngine 推理引擎
 */
InferEngine instance_get_engine(ModelInstance instance);

/**
 * @brief 设置实例优先级
 * 
 * @param instance 实例句柄
 * @param priority 优先级
 * @return int 0成功，其他失败
 */
int instance_set_priority(ModelInstance instance, uint32_t priority);

/**
 * @brief 获取实例优先级
 * 
 * @param instance 实例句柄
 * @return uint32_t 优先级
 */
uint32_t instance_get_priority(ModelInstance instance);

/**
 * @brief 实例池健康检查
 * 
 * @param pool 实例池
 * @return bool true健康，false不健康
 */
bool instance_pool_health_check(InstancePool pool);

/**
 * @brief 打印实例池调试信息
 * 
 * @param pool 实例池
 */
void instance_pool_print_debug(InstancePool pool);

/**
 * @brief 实例池性能分析
 * 
 * @param pool 实例池
 * @param report 性能报告输出
 * @return int 0成功，其他失败
 */
int instance_pool_analyze_performance(InstancePool pool, char** report);

/**
 * @brief 创建共享权重
 * 
 * @param manager 实例管理器
 * @param model_path 模型路径
 * @param shared_weights 共享权重输出
 * @return int 0成功，其他失败
 */
int instance_manager_create_shared_weights(InstanceManager manager, const char* model_path,
                                          MemoryHandle* shared_weights);

/**
 * @brief 销毁共享权重
 * 
 * @param manager 实例管理器
 * @param shared_weights 共享权重
 * @return int 0成功，其他失败
 */
int instance_manager_destroy_shared_weights(InstanceManager manager, MemoryHandle shared_weights);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_INSTANCE_MANAGER_H 