/**
 * @file rknn_device_example.c
 * @brief RKNN平台模型实例克隆实现示例
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "modyn.h"
// #include "rknn_api.h"  // RKNN官方SDK头文件

// RKNN模型实例数据结构
typedef struct {
    void *rknn_context;           // RKNN上下文
    void *shared_weight_context;  // RKNN共享权重上下文 (如果是原始实例)
    int is_clone;                 // 是否为克隆实例
    int clone_count;              // 克隆实例数量 (仅原始实例)
    struct rknn_model_instance *original; // 指向原始实例 (仅克隆实例)
} rknn_model_instance_t;

// RKNN设备私有数据
typedef struct {
    int device_id;
    void *rknn_driver_handle;
    // 其他RKNN特定数据
} rknn_device_private_t;

/**
 * @brief RKNN平台克隆模型实例
 */
static modyn_status_e rkmodyn_clone_model_instance(modyn_inference_device_handle_t device,
                                             modyn_model_instance_handle_t source_instance,
                                             modyn_model_instance_handle_t *cloned_instance) {
    (void)device; // 未使用参数
    
    rknn_model_instance_t *source = (rknn_model_instance_t*)source_instance;
    
    // 检查是否已经超过最大克隆数
    if (source->clone_count >= 8) { // RKNN默认最大8个实例
        return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
    }
    
    // 分配克隆实例结构
    rknn_model_instance_t *clone = malloc(sizeof(rknn_model_instance_t));
    if (!clone) return MODYN_ERROR_MEMORY_ALLOCATION;
    
    // 如果源实例还没有创建共享权重上下文，先创建
    if (!source->shared_weight_context && !source->is_clone) {
        // 调用RKNN API创建共享权重上下文
        // int ret = rknn_create_shared_weight_context(source->rknn_context, 
        //                                            &source->shared_weight_context, 8);
        // if (ret != RKNN_SUCC) {
        //     free(clone);
        //     return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
        // }
        
        source->shared_weight_context = (void*)0x12345678; // 模拟句柄
    }
    
    // 从共享权重创建新的RKNN实例
    // rknn_context clone_ctx;  
    // int ret = rknn_create_instance_from_shared(source->shared_weight_context, &clone_ctx);
    // if (ret != RKNN_SUCC) {
    //     free(clone);
    //     return MODYN_ERROR_MODEL_LOAD_FAILED;
    // }
    
    // 设置克隆实例
    clone->rknn_context = (void*)0x87654321; // clone_ctx;
    clone->shared_weight_context = NULL;
    clone->is_clone = 1;
    clone->clone_count = 0;
    clone->original = source;
    
    // 更新原始实例的克隆计数
    source->clone_count++;
    
    *cloned_instance = (modyn_model_instance_handle_t)clone;
    return MODYN_SUCCESS;
}

/**
 * @brief RKNN设备操作函数表
 */
static modyn_inference_device_ops_t rknn_device_ops = {
    .initialize = NULL, // rknn_device_initialize,
    .finalize = NULL,   // rknn_device_finalize,
    
    .load_model = NULL,   // rknn_load_model,
    .unload_model = NULL, // rknn_unload_model,
    
    .run_sync = NULL,  // rknn_run_sync,
    .run_async = NULL, // rknn_run_async,
    
    .allocate_tensor = NULL, // rknn_allocate_tensor,
    .free_tensor = NULL,     // rknn_free_tensor,
    .copy_tensor = NULL,     // rknn_copy_tensor,
    
    .get_performance = NULL, // rknn_get_performance,
    .reset_stats = NULL,     // rknn_reset_stats,
    
    // 模型实例克隆操作
    .clone_model_instance = rkmodyn_clone_model_instance,
    
    .extensions = NULL
};

/**
 * @brief RKNN设备工厂创建函数
 */
static modyn_status_e rknn_create_device(int device_id, modyn_inference_device_handle_t *device) {
    modyn_inference_device_t *dev = malloc(sizeof(modyn_inference_device_t));
    if (!dev) return MODYN_ERROR_MEMORY_ALLOCATION;
    
    // 设置设备信息
    dev->info.type = MODYN_DEVICE_NPU;
    snprintf(dev->info.name, sizeof(dev->info.name), "RKNN NPU %d", device_id);
    strcpy(dev->info.vendor, "Rockchip");
    
    // 设置能力标识
    dev->info.capabilities = MODYN_DEVICE_CAP_FLOAT16 | MODYN_DEVICE_CAP_INT8 | 
                            MODYN_DEVICE_CAP_CNN | MODYN_DEVICE_CAP_BATCH;
    
    // 分配私有数据
    rknn_device_private_t *priv = malloc(sizeof(rknn_device_private_t));
    priv->device_id = device_id;
    
    dev->ops = &rknn_device_ops;
    dev->private_data = priv;
    dev->ref_count = 1;
    dev->is_busy = 0;
    
    *device = (modyn_inference_device_handle_t)dev;
    return MODYN_SUCCESS;
}

/**
 * @brief RKNN设备工厂
 */
static modyn_device_factory_t rknn_device_factory = {
    .device_type = MODYN_DEVICE_NPU,
    .name = "RKNN",
    .create_device = rknn_create_device,
    .destroy_device = NULL, // rknn_destroy_device,
    .enumerate_devices = NULL, // rknn_enumerate_devices,
    .check_compatibility = NULL // rknn_check_compatibility
};

/**
 * @brief 注册RKNN设备工厂
 */
modyn_status_e register_rknn_device_factory(void) {
    return modyn_register_device_factory(&rknn_device_factory);
}

/**
 * @brief 使用示例：RKNN平台模型实例克隆
 */
void example_rknn_instance_cloning(void) {
    // 1. 创建RKNN设备
    modyn_inference_device_handle_t rknn_device;
    modyn_device_context_config_t config = {0};
    modyn_create_inference_device(MODYN_DEVICE_NPU, 0, &config, &rknn_device);
    
    // 2. 加载原始模型实例
    modyn_model_weight_handle_t weights;
    modyn_load_model_weights("model.rknn", NULL, NULL, &weights);
    
    modyn_model_instance_handle_t original_instance;
    modyn_create_model_instance(weights, MODYN_DEVICE_NPU, &original_instance);
    
    // 3. 检查是否支持克隆
    int supports_clone;
    modyn_status_e status = modyn_check_clone_support(original_instance, &supports_clone);
    
    if (status == MODYN_SUCCESS && supports_clone) {
        printf("RKNN设备支持模型实例克隆\n");
        
        // 4. 克隆多个实例 (共享权重)
        modyn_model_instance_handle_t cloned_instances[4];
        modyn_clone_config_t clone_config = {
            .enable_weight_sharing = 1,
            .max_concurrent_instances = 8,
            .platform_params = NULL
        };
        
        for (int i = 0; i < 4; i++) {
            status = modyn_clone_model_instance(original_instance, &clone_config, 
                                           &cloned_instances[i]);
            if (status != MODYN_SUCCESS) {
                printf("克隆实例 %d 失败: %d\n", i, status);
                break;
            }
        }
        
        // 5. 并发推理（每个实例独立执行，但共享权重）
        modyn_tensor_data_t inputs[5] = {0};  // 包括原始实例
        modyn_tensor_data_t *outputs[5];
        size_t num_outputs[5];
        
        // 原始实例推理
        modyn_run_inference_instance(original_instance, &inputs[0], 1, 
                                &outputs[0], &num_outputs[0]);
        
        // 克隆实例并发推理
        for (int i = 0; i < 4; i++) {
            modyn_run_inference_instance(cloned_instances[i], &inputs[i+1], 1, 
                                    &outputs[i+1], &num_outputs[i+1]);
        }
        
        // 6. 获取克隆信息
        int is_cloned, clone_count;
        modyn_get_clone_info(original_instance, &is_cloned, &clone_count);
        printf("原始实例克隆数量: %d\n", clone_count);
        
        // 7. 清理资源
        for (int i = 0; i < 4; i++) {
            modyn_destroy_model_instance(cloned_instances[i]);
        }
    } else {
        printf("RKNN设备不支持模型实例克隆\n");
    }
    
    // 清理原始资源
    modyn_destroy_model_instance(original_instance);
    modyn_unload_model_weights(weights);
    modyn_destroy_inference_device(rknn_device);
}