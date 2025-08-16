#include "core/memory_pool.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>

#define MEMORY_ALIGNMENT_DEFAULT 32
#define MEMORY_MAGIC_NUMBER 0x4D454D50  // "MEMP"

/**
 * @brief 内存块节点
 */
typedef struct memory_block_node_t {
    memory_block_t block;
    struct memory_block_node_t* next;
    struct memory_block_node_t* prev;
    uint32_t magic;
} memory_block_node_t;

/**
 * @brief 内存句柄结构
 */
struct memory_handle_internal_t {
    memory_block_node_t* block_node;
    memory_free_callback_t free_callback;
    void* callback_data;
    uint32_t magic;
};

/**
 * @brief 内存池内部结构
 */
struct memory_pool_internal_t {
    memory_pool_config_t config;
    void* memory_base;
    size_t memory_size;
    memory_block_node_t* free_blocks;
    memory_block_node_t* used_blocks;
    memory_pool_stats_t stats;
    pthread_mutex_t mutex;
    bool is_external;
    uint32_t magic;
};

// 内存对齐宏
#define ALIGN_SIZE(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))

// 获取当前时间戳
static uint64_t get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

// 创建内存块节点
static memory_block_node_t* create_block_node(void* ptr, size_t size, const char* tag) {
    memory_block_node_t* node = malloc(sizeof(memory_block_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(memory_block_node_t));
    node->block.ptr = ptr;
    node->block.size = size;
    node->block.is_free = true;
    node->block.alloc_time = get_timestamp();
    node->block.ref_count = 0;
    node->block.tag = tag ? strdup(tag) : NULL;
    node->magic = MEMORY_MAGIC_NUMBER;
    
    return node;
}

// 释放内存块节点
static void free_block_node(memory_block_node_t* node) {
    if (!node) return;
    
    free(node->block.tag);
    node->magic = 0;
    free(node);
}

// 从链表中移除节点
static void remove_from_list(memory_block_node_t** head, memory_block_node_t* node) {
    if (!node) return;
    
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        *head = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    }
    
    node->next = node->prev = NULL;
}

// 添加到链表头部
static void add_to_list(memory_block_node_t** head, memory_block_node_t* node) {
    if (!node) return;
    
    node->next = *head;
    node->prev = NULL;
    
    if (*head) {
        (*head)->prev = node;
    }
    
    *head = node;
}

// 分割内存块
static memory_block_node_t* split_block(memory_block_node_t* block, size_t size) {
    if (!block || block->block.size <= size) return NULL;
    
    size_t remaining_size = block->block.size - size;
    if (remaining_size < sizeof(void*)) return NULL;  // 剩余空间太小
    
    memory_block_node_t* new_block = create_block_node(
        (char*)block->block.ptr + size, 
        remaining_size, 
        NULL
    );
    
    if (new_block) {
        block->block.size = size;
        new_block->next = block->next;
        new_block->prev = block;
        
        if (block->next) {
            block->next->prev = new_block;
        }
        
        block->next = new_block;
    }
    
    return new_block;
}

// 合并相邻空闲块
static void merge_free_blocks(memory_pool_t pool) {
    if (!pool || !pool->free_blocks) return;
    
    memory_block_node_t* current = pool->free_blocks;
    
    while (current) {
        memory_block_node_t* next = current->next;
        
        // 检查是否与下一个块相邻
        if (next && (char*)current->block.ptr + current->block.size == next->block.ptr) {
            // 合并块
            current->block.size += next->block.size;
            current->next = next->next;
            
            if (next->next) {
                next->next->prev = current;
            }
            
            free_block_node(next);
        } else {
            current = next;
        }
    }
}

// 首次适应算法
static memory_block_node_t* first_fit_alloc(memory_pool_t pool, size_t size) {
    memory_block_node_t* current = pool->free_blocks;
    
    while (current) {
        if (current->block.size >= size) {
            remove_from_list(&pool->free_blocks, current);
            
            // 如果块太大，分割它
            memory_block_node_t* remaining = split_block(current, size);
            if (remaining) {
                add_to_list(&pool->free_blocks, remaining);
            }
            
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

// 最佳适应算法
static memory_block_node_t* best_fit_alloc(memory_pool_t pool, size_t size) {
    memory_block_node_t* current = pool->free_blocks;
    memory_block_node_t* best = NULL;
    size_t best_size = SIZE_MAX;
    
    while (current) {
        if (current->block.size >= size && current->block.size < best_size) {
            best = current;
            best_size = current->block.size;
            
            if (current->block.size == size) {
                break;  // 完美匹配
            }
        }
        current = current->next;
    }
    
    if (best) {
        remove_from_list(&pool->free_blocks, best);
        
        // 如果块太大，分割它
        memory_block_node_t* remaining = split_block(best, size);
        if (remaining) {
            add_to_list(&pool->free_blocks, remaining);
        }
    }
    
    return best;
}

memory_pool_t memory_pool_create(const memory_pool_config_t* config) {
    if (!config) return NULL;
    
    memory_pool_t pool = calloc(1, sizeof(struct memory_pool_internal_t));
    if (!pool) {
        LOG_ERROR("Failed to allocate memory pool");
        return NULL;
    }
    
    pool->config = *config;
    pool->magic = MEMORY_MAGIC_NUMBER;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize pool mutex");
        free(pool);
        return NULL;
    }
    
    // 分配内存
    if (config->external_memory) {
        pool->memory_base = config->external_memory;
        pool->memory_size = config->external_size;
        pool->is_external = true;
    } else {
        pool->memory_size = config->initial_size;
        pool->memory_base = malloc(pool->memory_size);
        if (!pool->memory_base) {
            LOG_ERROR("Failed to allocate pool memory");
            pthread_mutex_destroy(&pool->mutex);
            free(pool);
            return NULL;
        }
        pool->is_external = false;
    }
    
    // 创建初始空闲块
    memory_block_node_t* initial_block = create_block_node(
        pool->memory_base, 
        pool->memory_size, 
        "initial"
    );
    
    if (!initial_block) {
        LOG_ERROR("Failed to create initial block");
        if (!pool->is_external) {
            free(pool->memory_base);
        }
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
        return NULL;
    }
    
    pool->free_blocks = initial_block;
    
    // 初始化统计信息
    pool->stats.total_size = pool->memory_size;
    pool->stats.free_size = pool->memory_size;
    pool->stats.used_size = 0;
    pool->stats.peak_usage = 0;
    
    LOG_INFO("Memory pool created: type=%d, size=%zu, strategy=%d", 
             config->type, pool->memory_size, config->strategy);
    
    return pool;
}

void memory_pool_destroy(memory_pool_t pool) {
    if (!pool || pool->magic != MEMORY_MAGIC_NUMBER) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    // 释放所有块节点
    memory_block_node_t* current = pool->free_blocks;
    while (current) {
        memory_block_node_t* next = current->next;
        free_block_node(current);
        current = next;
    }
    
    current = pool->used_blocks;
    while (current) {
        memory_block_node_t* next = current->next;
        free_block_node(current);
        current = next;
    }
    
    // 释放内存
    if (!pool->is_external) {
        free(pool->memory_base);
    }
    
    pool->magic = 0;
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    
    LOG_INFO("Memory pool destroyed");
    free(pool);
}

memory_handle_t memory_pool_alloc(memory_pool_t pool, size_t size, size_t alignment, const char* tag) {
    if (!pool || pool->magic != MEMORY_MAGIC_NUMBER || size == 0) return NULL;
    
    pthread_mutex_lock(&pool->mutex);
    
    // 应用对齐
    if (alignment == 0) {
        alignment = pool->config.alignment > 0 ? pool->config.alignment : MEMORY_ALIGNMENT_DEFAULT;
    }
    
    size_t aligned_size = ALIGN_SIZE(size, alignment);
    
    // 根据策略选择分配算法
    memory_block_node_t* block = NULL;
    switch (pool->config.strategy) {
        case MEMORY_ALLOC_FIRST_FIT:
            block = first_fit_alloc(pool, aligned_size);
            break;
        case MEMORY_ALLOC_BEST_FIT:
            block = best_fit_alloc(pool, aligned_size);
            break;
        default:
            block = first_fit_alloc(pool, aligned_size);
            break;
    }
    
    if (!block) {
        pthread_mutex_unlock(&pool->mutex);
        LOG_ERROR("Failed to allocate memory: size=%zu", size);
        return NULL;
    }
    
    // 标记为已使用
    block->block.is_free = false;
    block->block.alloc_time = get_timestamp();
    block->block.ref_count = 1;
    block->block.alignment = alignment;
    
    if (tag) {
        free(block->block.tag);
        block->block.tag = strdup(tag);
    }
    
    // 移动到已使用列表
    add_to_list(&pool->used_blocks, block);
    
    // 更新统计信息
    pool->stats.used_size += aligned_size;
    pool->stats.free_size -= aligned_size;
    pool->stats.alloc_count++;
    pool->stats.active_blocks++;
    
    if (pool->stats.used_size > pool->stats.peak_usage) {
        pool->stats.peak_usage = pool->stats.used_size;
    }
    
    // 创建句柄
    memory_handle_t handle = calloc(1, sizeof(struct memory_handle_internal_t));
    if (!handle) {
        LOG_ERROR("Failed to create memory handle");
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }
    
    handle->block_node = block;
    handle->magic = MEMORY_MAGIC_NUMBER;
    
    pthread_mutex_unlock(&pool->mutex);
    
    LOG_DEBUG("Allocated memory: ptr=%p, size=%zu, tag=%s", 
              block->block.ptr, aligned_size, tag ? tag : "none");
    
    return handle;
}

int memory_pool_free(memory_pool_t pool, memory_handle_t handle) {
    if (!pool || !handle || pool->magic != MEMORY_MAGIC_NUMBER || 
        handle->magic != MEMORY_MAGIC_NUMBER) {
        return -1;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    memory_block_node_t* block = handle->block_node;
    if (!block || block->magic != MEMORY_MAGIC_NUMBER) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    
    // 检查引用计数
    if (block->block.ref_count > 1) {
        block->block.ref_count--;
        pthread_mutex_unlock(&pool->mutex);
        return 0;
    }
    
    // 调用释放回调
    if (handle->free_callback) {
        handle->free_callback(block->block.ptr, block->block.size, handle->callback_data);
    }
    
    // 从已使用列表中移除
    remove_from_list(&pool->used_blocks, block);
    
    // 标记为空闲
    block->block.is_free = true;
    block->block.ref_count = 0;
    
    // 添加到空闲列表
    add_to_list(&pool->free_blocks, block);
    
    // 更新统计信息
    pool->stats.used_size -= block->block.size;
    pool->stats.free_size += block->block.size;
    pool->stats.free_count++;
    pool->stats.active_blocks--;
    
    // 尝试合并空闲块
    merge_free_blocks(pool);
    
    handle->magic = 0;
    free(handle);
    
    pthread_mutex_unlock(&pool->mutex);
    
    LOG_DEBUG("Freed memory: ptr=%p, size=%zu", block->block.ptr, block->block.size);
    
    return 0;
}

void* memory_handle_get_ptr(memory_handle_t handle) {
    if (!handle || handle->magic != MEMORY_MAGIC_NUMBER) return NULL;
    
    memory_block_node_t* block = handle->block_node;
    if (!block || block->magic != MEMORY_MAGIC_NUMBER) return NULL;
    
    return block->block.ptr;
}

size_t memory_handle_get_size(memory_handle_t handle) {
    if (!handle || handle->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    memory_block_node_t* block = handle->block_node;
    if (!block || block->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    return block->block.size;
}

uint32_t memory_handle_ref(memory_handle_t handle) {
    if (!handle || handle->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    memory_block_node_t* block = handle->block_node;
    if (!block || block->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    return ++block->block.ref_count;
}

uint32_t memory_handle_unref(memory_handle_t handle) {
    if (!handle || handle->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    memory_block_node_t* block = handle->block_node;
    if (!block || block->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    if (block->block.ref_count > 0) {
        block->block.ref_count--;
    }
    
    return block->block.ref_count;
}

uint32_t memory_handle_get_ref_count(memory_handle_t handle) {
    if (!handle || handle->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    memory_block_node_t* block = handle->block_node;
    if (!block || block->magic != MEMORY_MAGIC_NUMBER) return 0;
    
    return block->block.ref_count;
}

void memory_handle_set_free_callback(memory_handle_t handle, memory_free_callback_t callback, void* user_data) {
    if (!handle || handle->magic != MEMORY_MAGIC_NUMBER) return;
    
    handle->free_callback = callback;
    handle->callback_data = user_data;
}

int memory_pool_get_stats(memory_pool_t pool, memory_pool_stats_t* stats) {
    if (!pool || !stats || pool->magic != MEMORY_MAGIC_NUMBER) return -1;
    
    pthread_mutex_lock(&pool->mutex);
    
    *stats = pool->stats;
    
    // 计算碎片率
    if (pool->stats.total_size > 0) {
        stats->fragmentation = (double)pool->stats.free_size / pool->stats.total_size;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    
    return 0;
}

memory_pool_t memory_pool_create_external(void* external_memory, size_t size, 
                                      memory_alloc_strategy_e strategy) {
    if (!external_memory || size == 0) return NULL;
    
    memory_pool_config_t config = {
        .type = MEMORY_POOL_EXTERNAL,
        .initial_size = size,
        .max_size = size,
        .grow_size = 0,
        .alignment = MEMORY_ALIGNMENT_DEFAULT,
        .strategy = strategy,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = external_memory,
        .external_size = size
    };
    
    return memory_pool_create(&config);
}

void memory_pool_print_debug(memory_pool_t pool) {
    if (!pool || pool->magic != MEMORY_MAGIC_NUMBER) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    printf("=== Memory Pool Debug Info ===\n");
    printf("Total Size: %zu bytes\n", pool->stats.total_size);
    printf("Used Size: %zu bytes\n", pool->stats.used_size);
    printf("Free Size: %zu bytes\n", pool->stats.free_size);
    printf("Peak Usage: %zu bytes\n", pool->stats.peak_usage);
    printf("Active Blocks: %u\n", pool->stats.active_blocks);
    printf("Alloc Count: %u\n", pool->stats.alloc_count);
    printf("Free Count: %u\n", pool->stats.free_count);
    
    printf("\nFree Blocks:\n");
    memory_block_node_t* current = pool->free_blocks;
    int free_count = 0;
    while (current) {
        printf("  Block %d: ptr=%p, size=%zu, tag=%s\n", 
               free_count++, current->block.ptr, current->block.size, 
               current->block.tag ? current->block.tag : "none");
        current = current->next;
    }
    
    printf("\nUsed Blocks:\n");
    current = pool->used_blocks;
    int used_count = 0;
    while (current) {
        printf("  Block %d: ptr=%p, size=%zu, refs=%u, tag=%s\n", 
               used_count++, current->block.ptr, current->block.size, 
               current->block.ref_count,
               current->block.tag ? current->block.tag : "none");
        current = current->next;
    }
    
    printf("===============================\n");
    
    pthread_mutex_unlock(&pool->mutex);
} 