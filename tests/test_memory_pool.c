#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include "core/memory_pool.h"
#include "utils/logger.h"

/**
 * @brief 内存池单元测试
 */

// 测试基本功能
void test_memory_pool_basic(void) {
    printf("测试内存池基本功能...\n");
    
    // 创建内存池
    memory_pool_config_t config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 10240,
        .max_size = 10240,
        .grow_size = 1024,
        .alignment = 8,
        .strategy = MEMORY_ALLOC_FIRST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    
    memory_pool_t pool = memory_pool_create(&config);
    assert(pool != NULL);
    
    // 获取统计信息
    memory_pool_stats_t stats;
    assert(memory_pool_get_stats(pool, &stats) == 0);
    assert(stats.total_size == 10240);
    assert(stats.free_size == 10240);
    assert(stats.used_size == 0);
    
    // 分配内存
    memory_handle_t handle1 = memory_pool_alloc(pool, 512, 8, "test1");
    assert(handle1 != NULL);
    assert(memory_handle_get_ptr(handle1) != NULL);
    assert(memory_handle_get_size(handle1) >= 512);
    
    // 分配更多内存
    memory_handle_t handle2 = memory_pool_alloc(pool, 256, 8, "test2");
    assert(handle2 != NULL);
    assert(memory_handle_get_ptr(handle2) != NULL);
    assert(memory_handle_get_size(handle2) >= 256);
    
    // 验证统计信息
    assert(memory_pool_get_stats(pool, &stats) == 0);
    assert(stats.used_size > 0);
    assert(stats.free_size < 10240);
    assert(stats.alloc_count == 2);
    assert(stats.active_blocks == 2);
    
    // 释放内存
    assert(memory_pool_free(pool, handle1) == 0);
    assert(memory_pool_free(pool, handle2) == 0);
    
    // 验证释放后的统计信息
    assert(memory_pool_get_stats(pool, &stats) == 0);
    assert(stats.free_count == 2);
    assert(stats.active_blocks == 0);
    
    // 销毁内存池
    memory_pool_destroy(pool);
    printf("✅ 基本功能测试通过\n");
}

// 测试边界条件
void test_memory_pool_boundary(void) {
    printf("测试内存池边界条件...\n");
    
    // 创建小内存池
    memory_pool_config_t config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 1024,
        .max_size = 1024,
        .grow_size = 0,
        .alignment = 8,
        .strategy = MEMORY_ALLOC_FIRST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    
    memory_pool_t pool = memory_pool_create(&config);
    assert(pool != NULL);
    
    // 尝试分配超过池大小的内存
    memory_handle_t handles[10];
    int success_count = 0;
    
    for (int i = 0; i < 10; i++) {
        handles[i] = memory_pool_alloc(pool, 200, 8, "test");
        if (handles[i] != NULL) {
            success_count++;
        }
    }
    
    // 应该能分配几个小块
    assert(success_count > 0);
    assert(success_count < 10);  // 但不能分配所有块
    
    // 尝试分配一个很大的块
    memory_handle_t big_handle = memory_pool_alloc(pool, 1024 * 1024, 8, "big_block");
    assert(big_handle == NULL);  // 应该失败
    
    // 释放已分配的内存
    for (int i = 0; i < 10; i++) {
        if (handles[i] != NULL) {
            memory_pool_free(pool, handles[i]);
        }
    }
    
    memory_pool_destroy(pool);
    printf("✅ 边界条件测试通过\n");
}

// 测试错误处理
void test_memory_pool_error_handling(void) {
    printf("测试内存池错误处理...\n");
    
    // 测试无效参数
    assert(memory_pool_create(NULL) == NULL);
    
    // 创建内存池
    memory_pool_config_t config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 1024,
        .max_size = 1024,
        .grow_size = 0,
        .alignment = 8,
        .strategy = MEMORY_ALLOC_FIRST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    
    memory_pool_t pool = memory_pool_create(&config);
    assert(pool != NULL);
    
    // 测试无效分配
    assert(memory_pool_alloc(pool, 0, 8, "test") == NULL);
    assert(memory_pool_alloc(NULL, 100, 8, "test") == NULL);
    
    // 测试无效释放
    assert(memory_pool_free(pool, NULL) == -1);
    assert(memory_pool_free(NULL, NULL) == -1);
    
    // 测试无效句柄操作
    assert(memory_handle_get_ptr(NULL) == NULL);
    assert(memory_handle_get_size(NULL) == 0);
    assert(memory_handle_ref(NULL) == 0);
    assert(memory_handle_unref(NULL) == 0);
    assert(memory_handle_get_ref_count(NULL) == 0);
    
    memory_pool_destroy(pool);
    printf("✅ 错误处理测试通过\n");
}

// 测试统计信息
void test_memory_pool_stats(void) {
    printf("测试内存池统计信息...\n");
    
    memory_pool_config_t config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 5120,
        .max_size = 5120,
        .grow_size = 0,
        .alignment = 8,
        .strategy = MEMORY_ALLOC_FIRST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    
    memory_pool_t pool = memory_pool_create(&config);
    assert(pool != NULL);
    
    memory_pool_stats_t stats;
    assert(memory_pool_get_stats(pool, &stats) == 0);
    assert(stats.total_size == 5120);
    assert(stats.used_size == 0);
    assert(stats.free_size == 5120);
    assert(stats.alloc_count == 0);
    assert(stats.free_count == 0);
    assert(stats.active_blocks == 0);
    
    // 分配和释放内存
    memory_handle_t handle1 = memory_pool_alloc(pool, 1024, 8, "test1");
    memory_handle_t handle2 = memory_pool_alloc(pool, 1024, 8, "test2");
    
    assert(memory_pool_get_stats(pool, &stats) == 0);
    assert(stats.alloc_count == 2);
    assert(stats.active_blocks == 2);
    assert(stats.used_size > 0);
    
    memory_pool_free(pool, handle1);
    memory_pool_free(pool, handle2);
    
    assert(memory_pool_get_stats(pool, &stats) == 0);
    assert(stats.free_count == 2);
    assert(stats.active_blocks == 0);
    
    memory_pool_destroy(pool);
    printf("✅ 统计信息测试通过\n");
}

// 线程测试数据
typedef struct {
    memory_pool_t pool;
    int thread_id;
    int iterations;
    int success_count;
} ThreadTestData;

// 线程测试函数
void* thread_test_func(void* arg) {
    ThreadTestData* data = (ThreadTestData*)arg;
    data->success_count = 0;
    
    for (int i = 0; i < data->iterations; i++) {
        // 分配内存
        memory_handle_t handle = memory_pool_alloc(data->pool, 64 + (i % 256), 8, "thread_test");
        if (handle != NULL) {
            data->success_count++;
            
            // 模拟使用内存
            void* ptr = memory_handle_get_ptr(handle);
            if (ptr != NULL) {
                memset(ptr, data->thread_id, 64);
                usleep(1000);  // 1ms
            }
            
            // 释放内存
            memory_pool_free(data->pool, handle);
        }
    }
    
    return NULL;
}

// 测试线程安全
void test_memory_pool_thread_safety(void) {
    printf("测试内存池线程安全...\n");
    
    memory_pool_config_t config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 1024 * 20,  // 20KB
        .max_size = 1024 * 20,
        .grow_size = 1024 * 10,
        .alignment = 8,
        .strategy = MEMORY_ALLOC_FIRST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    
    memory_pool_t pool = memory_pool_create(&config);
    assert(pool != NULL);
    
    const int num_threads = 4;
    const int iterations = 100;
    
    pthread_t threads[num_threads];
    ThreadTestData thread_data[num_threads];
    
    // 创建线程
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].pool = pool;
        thread_data[i].thread_id = i;
        thread_data[i].iterations = iterations;
        thread_data[i].success_count = 0;
        
        int ret = pthread_create(&threads[i], NULL, thread_test_func, &thread_data[i]);
        assert(ret == 0);
    }
    
    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        printf("线程 %d 成功分配/释放 %d/%d 次\n", 
               i, thread_data[i].success_count, iterations);
    }
    
    // 检查最终状态
    memory_pool_stats_t stats;
    assert(memory_pool_get_stats(pool, &stats) == 0);
    printf("最终统计: 总分配=%u, 总释放=%u, 使用中=%zu bytes\n",
           stats.alloc_count, stats.free_count, stats.used_size);
    
    memory_pool_destroy(pool);
    
    printf("✅ 线程安全测试通过\n");
}

int main(void) {
    printf("=== 内存池单元测试 ===\n");
    
    // 初始化日志系统
    logger_init(LOG_LEVEL_INFO, NULL);
    
    test_memory_pool_basic();
    test_memory_pool_boundary();
    test_memory_pool_error_handling();
    test_memory_pool_stats();
    test_memory_pool_thread_safety();
    
    printf("\n=== 所有测试通过 ===\n");
    
    // 清理日志系统
    logger_cleanup();
    
    return 0;
} 