#ifndef MODYN_CORE_MEMORY_POOL_H
#define MODYN_CORE_MEMORY_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 内存池类型
 */
typedef enum {
    MEMORY_POOL_CPU = 0,        /**< CPU内存池 */
    MEMORY_POOL_GPU,            /**< GPU内存池 */
    MEMORY_POOL_SHARED,         /**< 共享内存池 */
    MEMORY_POOL_EXTERNAL        /**< 外部内存池 */
} memory_pool_type_e;

/**
 * @brief 内存分配策略
 */
typedef enum {
    MEMORY_ALLOC_FIRST_FIT = 0, /**< 首次适应 */
    MEMORY_ALLOC_BEST_FIT,      /**< 最佳适应 */
    MEMORY_ALLOC_WORST_FIT,     /**< 最坏适应 */
    MEMORY_ALLOC_BUDDY          /**< 伙伴系统 */
} memory_alloc_strategy_e;

/**
 * @brief 内存块信息
 */
typedef struct {
    void* ptr;                  /**< 内存地址 */
    size_t size;                /**< 块大小 */
    size_t alignment;           /**< 对齐要求 */
    bool is_free;               /**< 是否空闲 */
    uint64_t alloc_time;        /**< 分配时间 */
    uint32_t ref_count;         /**< 引用计数 */
    char* tag;                  /**< 标签 */
} memory_block_t;

/**
 * @brief 内存池配置
 */
typedef struct {
    memory_pool_type_e type;    /**< 内存池类型 */
    size_t initial_size;        /**< 初始大小 */
    size_t max_size;            /**< 最大大小 */
    size_t grow_size;           /**< 增长大小 */
    size_t alignment;           /**< 默认对齐 */
    memory_alloc_strategy_e strategy; /**< 分配策略 */
    bool enable_tracking;       /**< 启用跟踪 */
    bool enable_debug;          /**< 启用调试 */
    void* external_memory;      /**< 外部内存（可选） */
    size_t external_size;       /**< 外部内存大小 */
} memory_pool_config_t;

/**
 * @brief 内存池统计信息
 */
typedef struct {
    size_t total_size;          /**< 总大小 */
    size_t used_size;           /**< 已使用大小 */
    size_t free_size;           /**< 空闲大小 */
    size_t peak_usage;          /**< 峰值使用量 */
    uint32_t alloc_count;       /**< 分配次数 */
    uint32_t free_count;        /**< 释放次数 */
    uint32_t active_blocks;     /**< 活跃块数 */
    double fragmentation;       /**< 碎片率 */
} memory_pool_stats_t;

/**
 * @brief 内存池句柄
 */
typedef struct memory_pool_internal_t* memory_pool_t;

/**
 * @brief 内存句柄
 */
typedef struct memory_handle_internal_t* memory_handle_t;

/**
 * @brief 内存释放回调函数
 */
typedef void (*memory_free_callback_t)(void* ptr, size_t size, void* user_data);



/**
 * @brief 创建内存池
 * 
 * @param config 配置信息
 * @return memory_pool_t 内存池实例
 */
memory_pool_t memory_pool_create(const memory_pool_config_t* config);

/**
 * @brief 销毁内存池
 * 
 * @param pool 内存池实例
 */
void memory_pool_destroy(memory_pool_t pool);

/**
 * @brief 分配内存
 * 
 * @param pool 内存池实例
 * @param size 分配大小
 * @param alignment 对齐要求（0使用默认对齐）
 * @param tag 标签（可选）
 * @return memory_handle_t 内存句柄
 */
memory_handle_t memory_pool_alloc(memory_pool_t pool, size_t size, size_t alignment, const char* tag);

/**
 * @brief 重新分配内存
 * 
 * @param pool 内存池实例
 * @param handle 原内存句柄
 * @param new_size 新大小
 * @return memory_handle_t 新内存句柄
 */
memory_handle_t memory_pool_realloc(memory_pool_t pool, memory_handle_t handle, size_t new_size);

/**
 * @brief 释放内存
 * 
 * @param pool 内存池实例
 * @param handle 内存句柄
 * @return int 0成功，其他失败
 */
int memory_pool_free(memory_pool_t pool, memory_handle_t handle);

/**
 * @brief 获取内存地址
 * 
 * @param handle 内存句柄
 * @return void* 内存地址
 */
void* memory_handle_get_ptr(memory_handle_t handle);

/**
 * @brief 获取内存大小
 * 
 * @param handle 内存句柄
 * @return size_t 内存大小
 */
size_t memory_handle_get_size(memory_handle_t handle);

/**
 * @brief 增加引用计数
 * 
 * @param handle 内存句柄
 * @return uint32_t 新的引用计数
 */
uint32_t memory_handle_ref(memory_handle_t handle);

/**
 * @brief 减少引用计数
 * 
 * @param handle 内存句柄
 * @return uint32_t 新的引用计数
 */
uint32_t memory_handle_unref(memory_handle_t handle);

/**
 * @brief 获取引用计数
 * 
 * @param handle 内存句柄
 * @return uint32_t 引用计数
 */
uint32_t memory_handle_get_ref_count(memory_handle_t handle);

/**
 * @brief 设置释放回调
 * 
 * @param handle 内存句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void memory_handle_set_free_callback(memory_handle_t handle, memory_free_callback_t callback, void* user_data);

/**
 * @brief 内存池统计信息
 * 
 * @param pool 内存池实例
 * @param stats 统计信息输出
 * @return int 0成功，其他失败
 */
int memory_pool_get_stats(memory_pool_t pool, memory_pool_stats_t* stats);

/**
 * @brief 内存池压缩
 * 
 * @param pool 内存池实例
 * @return int 0成功，其他失败
 */
int memory_pool_compact(memory_pool_t pool);

/**
 * @brief 重置内存池
 * 
 * @param pool 内存池实例
 * @return int 0成功，其他失败
 */
int memory_pool_reset(memory_pool_t pool);

/**
 * @brief 设置内存池大小限制
 * 
 * @param pool 内存池实例
 * @param max_size 最大大小
 * @return int 0成功，其他失败
 */
int memory_pool_set_size_limit(memory_pool_t pool, size_t max_size);

/**
 * @brief 获取内存块信息
 * 
 * @param pool 内存池实例
 * @param blocks 内存块数组输出
 * @param count 内存块数量输出
 * @return int 0成功，其他失败
 */
int memory_pool_get_blocks(memory_pool_t pool, memory_block_t** blocks, uint32_t* count);

/**
 * @brief 打印调试信息
 * 
 * @param pool 内存池实例
 */
void memory_pool_print_debug(memory_pool_t pool);

/**
 * @brief 检查内存池完整性
 * 
 * @param pool 内存池实例
 * @return bool true完整，false不完整
 */
bool memory_pool_check_integrity(memory_pool_t pool);

/**
 * @brief 创建外部内存池
 * 
 * @param external_memory 外部内存地址
 * @param size 内存大小
 * @param strategy 分配策略
 * @return memory_pool_t 内存池实例
 */
memory_pool_t memory_pool_create_external(void* external_memory, size_t size, 
                                      memory_alloc_strategy_e strategy);

/**
 * @brief 预分配内存
 * 
 * @param pool 内存池实例
 * @param size 预分配大小
 * @return int 0成功，其他失败
 */
int memory_pool_preallocate(memory_pool_t pool, size_t size);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_MEMORY_POOL_H