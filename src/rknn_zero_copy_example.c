/**
 * @file rknn_zero_copy_example.c
 * @brief RKNN平台零拷贝推理实现示例
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "modyn.h"
// #include "rknn_api.h"  // RKNN官方SDK头文件

// ======================= RKNN零拷贝私有结构 ======================= //

/**
 * @brief RKNN零拷贝内存区域私有数据
 */
typedef struct {
    void *rknn_mem_handle;      // RKNN内存句柄
    int dma_fd;                 // DMA buffer文件描述符
    void *mapped_addr;          // 映射的虚拟地址
    size_t aligned_size;        // 对齐后的实际大小
} rknn_zero_copy_private_t;

/**
 * @brief RKNN模型实例零拷贝扩展
 */
typedef struct {
    void *rknn_context;         // RKNN上下文
    
    // 输入输出规格信息
    int num_inputs;
    int num_outputs;
    void *input_attrs;          // rknn_input_output_num + rknn_tensor_attr
    void *output_attrs;         // rknn_input_output_num + rknn_tensor_attr
    
    // 零拷贝缓冲区池
    modyn_zero_copy_buffer_pool_t *buffer_pool;
    
} rknn_zero_copy_instance_t;

// ======================= RKNN零拷贝内存操作 ======================= //

/**
 * @brief 分配RKNN零拷贝内存区域
 */
static modyn_status_e rknn_allocate_zero_copy_region(modyn_inference_device_handle_t device,
                                                  size_t size,
                                                  size_t alignment,
                                                  modyn_memory_type_e mem_type,
                                                  modyn_zero_copy_memory_region_t *region) {
    (void)device; // 未使用参数
    
    // 分配私有数据
    rknn_zero_copy_private_t *priv = malloc(sizeof(rknn_zero_copy_private_t));
    if (!priv) return MODYN_ERROR_MEMORY_ALLOCATION;
    
    // 对齐大小到页边界
    size_t page_size = getpagesize();
    size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
    
    // 调用RKNN API分配DMA buffer
    // rknn_mem_size mem_size;
    // mem_size.size = aligned_size;
    // void *rknn_mem = rknn_mem_create(&mem_size, RKNN_MEM_TYPE_DMA_BUFFER);
    // if (!rknn_mem) {
    //     free(priv);
    //     return MODYN_ERROR_MEMORY_ALLOCATION;
    // }
    
    // 模拟RKNN内存分配
    void *rknn_mem = malloc(aligned_size);
    if (!rknn_mem) {
        free(priv);
        return MODYN_ERROR_MEMORY_ALLOCATION;
    }
    
    // 获取DMA buffer的文件描述符
    // int dma_fd = rknn_mem_get_fd(rknn_mem);
    int dma_fd = -1; // 模拟值
    
    // 映射内存到用户空间
    void *mapped_addr = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, dma_fd, 0);
    if (mapped_addr == MAP_FAILED) {
        // rknn_mem_destroy(rknn_mem);
        free(rknn_mem);
        free(priv);
        return MODYN_ERROR_MEMORY_ALLOCATION;
    }
    
    // 设置私有数据
    priv->rknn_mem_handle = rknn_mem;
    priv->dma_fd = dma_fd;
    priv->mapped_addr = mapped_addr;
    priv->aligned_size = aligned_size;
    
    // 设置内存区域信息
    region->virtual_addr = mapped_addr;
    region->physical_addr = 0; // 由RKNN驱动管理
    region->fd = dma_fd;
    region->size = size;
    region->alignment = alignment;
    region->mem_type = mem_type;
    region->device_type = MODYN_DEVICE_NPU;
    region->is_coherent = 0; // RKNN通常需要手动同步
    region->platform_handle = priv;
    
    printf("分配RKNN零拷贝内存: size=%zu, aligned=%zu, fd=%d, addr=%p\n",
           size, aligned_size, dma_fd, mapped_addr);
    
    return MODYN_SUCCESS;
}

/**
 * @brief 释放RKNN零拷贝内存区域
 */
static modyn_status_e rknn_free_zero_copy_region(modyn_inference_device_handle_t device,
                                             modyn_zero_copy_memory_region_t *region) {
    (void)device; // 未使用参数
    
    if (!region || !region->platform_handle) return MODYN_ERROR_INVALID_ARGUMENT;
    
    rknn_zero_copy_private_t *priv = (rknn_zero_copy_private_t*)region->platform_handle;
    
    // 取消内存映射
    if (priv->mapped_addr && priv->mapped_addr != MAP_FAILED) {
        munmap(priv->mapped_addr, priv->aligned_size);
    }
    
    // 释放RKNN内存
    if (priv->rknn_mem_handle) {
        // rknn_mem_destroy(priv->rknn_mem_handle);
        free(priv->rknn_mem_handle);
    }
    
    free(priv);
    memset(region, 0, sizeof(*region));
    
    printf("释放RKNN零拷贝内存\n");
    return MODYN_SUCCESS;
}

/**
 * @brief 同步RKNN内存区域
 */
static modyn_status_e rknn_sync_zero_copy_region(modyn_zero_copy_memory_region_t *region,
                                             int to_device) {
    if (!region || !region->platform_handle) return MODYN_ERROR_INVALID_ARGUMENT;
    
    rknn_zero_copy_private_t *priv = (rknn_zero_copy_private_t*)region->platform_handle;
    
    if (to_device) {
        // CPU -> NPU: 刷新CPU缓存，确保数据写入内存
        // rknn_mem_sync(priv->rknn_mem_handle, RKNN_MEM_SYNC_FLUSH);
        printf("同步内存到RKNN NPU设备\n");
    } else {
        // NPU -> CPU: 无效化CPU缓存，确保读取最新数据  
        // rknn_mem_sync(priv->rknn_mem_handle, RKNN_MEM_SYNC_INVALIDATE);
        printf("同步内存到CPU\n");
    }
    
    return MODYN_SUCCESS;
}

/**
 * @brief 获取RKNN内存区域文件描述符
 */
static modyn_status_e rknn_get_region_fd(modyn_zero_copy_memory_region_t *region, int *fd) {
    if (!region || !fd) return MODYN_ERROR_INVALID_ARGUMENT;
    
    *fd = region->fd;
    return MODYN_SUCCESS;
}

// RKNN零拷贝内存操作函数表
static modyn_zero_copy_memory_ops_t rknn_zero_copy_ops = {
    .allocate_region = rknn_allocate_zero_copy_region,
    .free_region = rknn_free_zero_copy_region,
    .map_region = NULL,   // 在allocate时已映射
    .unmap_region = NULL, // 在free时取消映射
    .sync_region = rknn_sync_zero_copy_region,
    .get_fd = rknn_get_region_fd
};

// ======================= RKNN零拷贝缓冲区池 ======================= //

/**
 * @brief 创建RKNN零拷贝缓冲区池
 */
static modyn_status_e rknn_create_zero_copy_pool(modyn_inference_device_handle_t device,
                                             modyn_model_instance_handle_t instance,
                                             modyn_zero_copy_buffer_pool_t **pool) {
    rknn_zero_copy_instance_t *rknn_instance = (rknn_zero_copy_instance_t*)instance;
    
    // 分配缓冲区池结构
    modyn_zero_copy_buffer_pool_t *buffer_pool = malloc(sizeof(modyn_zero_copy_buffer_pool_t));
    if (!buffer_pool) return MODYN_ERROR_MEMORY_ALLOCATION;
    
    memset(buffer_pool, 0, sizeof(*buffer_pool));
    
    // 获取模型输入输出信息
    // rknn_input_output_num io_num;
    // rknn_query(rknn_instance->rknn_context, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    
    // 模拟输入输出数量
    int num_inputs = 1;   // io_num.n_input;
    int num_outputs = 1;  // io_num.n_output;
    
    // 分配输入输出区域数组
    buffer_pool->input_regions = malloc(num_inputs * sizeof(modyn_zero_copy_memory_region_t));
    buffer_pool->output_regions = malloc(num_outputs * sizeof(modyn_zero_copy_memory_region_t));
    
    if (!buffer_pool->input_regions || !buffer_pool->output_regions) {
        free(buffer_pool->input_regions);
        free(buffer_pool->output_regions);
        free(buffer_pool);
        return MODYN_ERROR_MEMORY_ALLOCATION;
    }
    
    // 为每个输入分配零拷贝内存
    for (int i = 0; i < num_inputs; i++) {
        // 获取输入tensor属性
        // rknn_tensor_attr input_attr;
        // input_attr.index = i;
        // rknn_query(rknn_instance->rknn_context, RKNN_QUERY_INPUT_ATTR, &input_attr, sizeof(input_attr));
        
        // 计算内存大小 (模拟: 224x224x3 RGB图像)
        size_t input_size = 224 * 224 * 3 * sizeof(uint8_t);
        
        modyn_status_e status = rknn_allocate_zero_copy_region(device, input_size, 64,
                                                           MODYN_MEMORY_ZERO_COPY,
                                                           &buffer_pool->input_regions[i]);
        if (status != MODYN_SUCCESS) {
            // 清理已分配的内存
            for (int j = 0; j < i; j++) {
                rknn_free_zero_copy_region(device, &buffer_pool->input_regions[j]);
            }
            free(buffer_pool->input_regions);
            free(buffer_pool->output_regions);
            free(buffer_pool);
            return status;
        }
        
        printf("输入[%d]: 分配 %zu bytes 零拷贝内存，地址: %p\n",
               i, input_size, buffer_pool->input_regions[i].virtual_addr);
    }
    
    // 为每个输出分配零拷贝内存
    for (int i = 0; i < num_outputs; i++) {
        // 获取输出tensor属性
        // rknn_tensor_attr output_attr;
        // output_attr.index = i;
        // rknn_query(rknn_instance->rknn_context, RKNN_QUERY_OUTPUT_ATTR, &output_attr, sizeof(output_attr));
        
        // 计算内存大小 (模拟: 1000分类输出)
        size_t output_size = 1000 * sizeof(float);
        
        modyn_status_e status = rknn_allocate_zero_copy_region(device, output_size, 64,
                                                           MODYN_MEMORY_ZERO_COPY,
                                                           &buffer_pool->output_regions[i]);
        if (status != MODYN_SUCCESS) {
            // 清理已分配的内存
            for (int j = 0; j < num_inputs; j++) {
                rknn_free_zero_copy_region(device, &buffer_pool->input_regions[j]);
            }
            for (int j = 0; j < i; j++) {
                rknn_free_zero_copy_region(device, &buffer_pool->output_regions[j]);
            }
            free(buffer_pool->input_regions);
            free(buffer_pool->output_regions);
            free(buffer_pool);
            return status;
        }
        
        printf("输出[%d]: 分配 %zu bytes 零拷贝内存，地址: %p\n",
               i, output_size, buffer_pool->output_regions[i].virtual_addr);
    }
    
    // 设置缓冲区池信息
    buffer_pool->num_inputs = num_inputs;
    buffer_pool->num_outputs = num_outputs;
    buffer_pool->is_allocated = 1;
    buffer_pool->owner_instance = instance;
    buffer_pool->sync_objects = NULL;
    
    *pool = buffer_pool;
    
    printf("成功创建RKNN零拷贝缓冲区池: %d输入, %d输出\n", num_inputs, num_outputs);
    return MODYN_SUCCESS;
}

/**
 * @brief 销毁RKNN零拷贝缓冲区池
 */
static modyn_status_e rknn_destroy_zero_copy_pool(modyn_inference_device_handle_t device,
                                              modyn_zero_copy_buffer_pool_t *pool) {
    if (!pool) return MODYN_ERROR_INVALID_ARGUMENT;
    
    // 释放输入内存区域
    for (size_t i = 0; i < pool->num_inputs; i++) {
        rknn_free_zero_copy_region(device, &pool->input_regions[i]);
    }
    
    // 释放输出内存区域
    for (size_t i = 0; i < pool->num_outputs; i++) {
        rknn_free_zero_copy_region(device, &pool->output_regions[i]);
    }
    
    free(pool->input_regions);
    free(pool->output_regions);
    free(pool);
    
    printf("销毁RKNN零拷贝缓冲区池\n");
    return MODYN_SUCCESS;
}

/**
 * @brief RKNN零拷贝推理
 */
static modyn_status_e rknn_run_inference_zero_copy(modyn_inference_device_handle_t device,
                                               modyn_model_instance_handle_t instance,
                                               modyn_zero_copy_buffer_pool_t *pool) {
    (void)device; // 未使用参数
    
    rknn_zero_copy_instance_t *rknn_instance = (rknn_zero_copy_instance_t*)instance;
    
    if (!pool || !pool->is_allocated) {
        return MODYN_ERROR_INVALID_ARGUMENT;
    }
    
    printf("开始RKNN零拷贝推理...\n");
    
    // 1. 同步输入数据到NPU
    for (size_t i = 0; i < pool->num_inputs; i++) {
        rknn_sync_zero_copy_region(&pool->input_regions[i], 1); // to_device=1
    }
    
    // 2. 设置RKNN输入（零拷贝）
    // rknn_input inputs[pool->num_inputs];
    // for (size_t i = 0; i < pool->num_inputs; i++) {
    //     inputs[i].index = i;
    //     inputs[i].type = RKNN_TENSOR_UINT8;
    //     inputs[i].size = pool->input_regions[i].size;
    //     inputs[i].fmt = RKNN_TENSOR_NCHW;
    //     inputs[i].pass_through = 0;
    //     inputs[i].buf = pool->input_regions[i].virtual_addr; // 零拷贝地址
    // }
    
    // 3. 设置RKNN输出（零拷贝）
    // rknn_output outputs[pool->num_outputs];
    // for (size_t i = 0; i < pool->num_outputs; i++) {
    //     outputs[i].index = i;
    //     outputs[i].want_float = 1;
    //     outputs[i].is_prealloc = 1; // 使用预分配内存
    //     outputs[i].buf = pool->output_regions[i].virtual_addr; // 零拷贝地址
    //     outputs[i].size = pool->output_regions[i].size;
    // }
    
    // 4. 执行推理（零拷贝模式）
    // int ret = rknn_inputs_set(rknn_instance->rknn_context, pool->num_inputs, inputs);
    // if (ret != RKNN_SUCC) return NN_ERROR_PIPELINE_EXECUTION;
    
    // ret = rknn_run(rknn_instance->rknn_context, NULL);
    // if (ret != RKNN_SUCC) return NN_ERROR_PIPELINE_EXECUTION;
    
    // ret = rknn_outputs_get(rknn_instance->rknn_context, pool->num_outputs, outputs, NULL);
    // if (ret != RKNN_SUCC) return NN_ERROR_PIPELINE_EXECUTION;
    
    // 模拟推理耗时
    usleep(5000); // 5ms
    
    // 5. 同步输出数据到CPU
    for (size_t i = 0; i < pool->num_outputs; i++) {
        rknn_sync_zero_copy_region(&pool->output_regions[i], 0); // to_device=0
    }
    
    printf("RKNN零拷贝推理完成\n");
    return MODYN_SUCCESS;
}

// ======================= 使用示例 ======================= //

/**
 * @brief 零拷贝推理使用示例
 */
void example_rknn_zero_copy_inference(void) {
    printf("=== RKNN零拷贝推理示例 ===\n");
    
    // 1. 创建RKNN设备和模型实例 (简化)
    modyn_inference_device_handle_t device = (modyn_inference_device_handle_t)0x12345678;
    modyn_model_instance_handle_t instance = (modyn_model_instance_handle_t)0x87654321;
    
    // 2. 检查是否支持零拷贝
    int supports_zero_copy;
    modyn_status_e status = modyn_check_zero_copy_support(instance, &supports_zero_copy);
    
    if (status != MODYN_SUCCESS || !supports_zero_copy) {
        printf("模型实例不支持零拷贝\n");
        return;
    }
    
    printf("✓ 模型实例支持零拷贝\n");
    
    // 3. 创建零拷贝缓冲区池
    modyn_zero_copy_buffer_pool_t *pool;
    status = rknn_create_zero_copy_pool(device, instance, &pool);
    
    if (status != MODYN_SUCCESS) {
        printf("创建零拷贝缓冲区池失败: %d\n", status);
        return;
    }
    
    printf("✓ 零拷贝缓冲区池创建成功\n");
    
    // 4. 获取输入缓冲区并填充数据
    modyn_zero_copy_memory_region_t *input_region;
    status = modyn_get_input_buffer_region(pool, 0, &input_region);
    
    if (status == MODYN_SUCCESS) {
        // 直接写入零拷贝内存区域
        uint8_t *input_data = (uint8_t*)input_region->virtual_addr;
        
        // 模拟填充图像数据 (224x224x3)
        for (size_t i = 0; i < input_region->size; i++) {
            input_data[i] = (uint8_t)(i % 256);
        }
        
        printf("✓ 输入数据已写入零拷贝内存: %p, size: %zu\n",
               input_data, input_region->size);
        
        // 获取文件描述符 (可用于进程间共享)
        int input_fd;
        if (rknn_get_region_fd(input_region, &input_fd) == MODYN_SUCCESS) {
            printf("✓ 输入缓冲区文件描述符: %d\n", input_fd);
        }
    }
    
    // 5. 执行零拷贝推理
    status = rknn_run_inference_zero_copy(device, instance, pool);
    
    if (status == MODYN_SUCCESS) {
        printf("✓ 零拷贝推理执行成功\n");
        
        // 6. 获取输出结果
        modyn_zero_copy_memory_region_t *output_region;
        status = nn_get_output_buffer_region(pool, 0, &output_region);
        
        if (status == MODYN_SUCCESS) {
            float *output_data = (float*)output_region->virtual_addr;
            
            printf("✓ 输出结果 (前5个值): ");
            for (int i = 0; i < 5 && i < (int)(output_region->size / sizeof(float)); i++) {
                printf("%.4f ", output_data[i]);
            }
            printf("\n");
        }
    } else {
        printf("✗ 零拷贝推理失败: %d\n", status);
    }
    
    // 7. 多次推理测试 (重复使用缓冲区)
    printf("\n=== 连续推理性能测试 ===\n");
    for (int i = 0; i < 5; i++) {
        status = rknn_run_inference_zero_copy(device, instance, pool);
        if (status == MODYN_SUCCESS) {
            printf("推理 %d: 成功\n", i + 1);
        } else {
            printf("推理 %d: 失败 (%d)\n", i + 1, status);
            break;
        }
    }
    
    // 8. 清理资源
    rknn_destroy_zero_copy_pool(device, pool);
    printf("✓ 资源清理完成\n");
}

/**
 * @brief 零拷贝内存共享示例 (多进程)
 */
void example_zero_copy_memory_sharing(void) {
    printf("\n=== 零拷贝内存共享示例 ===\n");
    
    // 创建共享的零拷贝内存区域
    modyn_zero_copy_memory_region_t shared_region;
    modyn_inference_device_handle_t device = (modyn_inference_device_handle_t)0x12345678;
    
    modyn_status_e status = rknn_allocate_zero_copy_region(device, 4096, 64,
                                                       MODYN_MEMORY_ZERO_COPY,
                                                       &shared_region);
    
    if (status == MODYN_SUCCESS) {
        printf("✓ 创建共享零拷贝内存: %p, fd: %d\n",
               shared_region.virtual_addr, shared_region.fd);
        
        // 写入测试数据
        uint32_t *data = (uint32_t*)shared_region.virtual_addr;
        for (int i = 0; i < 10; i++) {
            data[i] = i * 100;
        }
        
        // 同步到设备
        rknn_sync_zero_copy_region(&shared_region, 1);
        
        printf("✓ 数据已写入并同步到设备\n");
        printf("共享内存文件描述符: %d (可传递给其他进程)\n", shared_region.fd);
        
        // 清理
        rknn_free_zero_copy_region(device, &shared_region);
    } else {
        printf("✗ 创建共享内存失败: %d\n", status);
    }
}