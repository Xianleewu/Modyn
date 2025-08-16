#include "core/model_manager.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

/**
 * @brief 模型实例结构
 */
typedef struct ModelInstance {
    char* model_id;
    char* model_path;
    char* version;
    model_status_e status;
    infer_engine_t engine;
    infer_backend_type_e backend;
    uint32_t ref_count;
    uint64_t inference_count;
    double total_latency;
    uint32_t max_instances;
    pthread_mutex_t mutex;
    struct ModelInstance* next;
} ModelInstance;

/**
 * @brief 模型管理器结构
 */
typedef struct ModelManager {
    ModelInstance** models;
    uint32_t count;
    uint32_t capacity;
    pthread_mutex_t mutex;
} model_manager_t;

/**
 * @brief 模型句柄结构
 */
struct ModelHandle {
    ModelInstance* instance;
    model_manager_t* manager;
};

model_manager_t* model_manager_create(void) {
    model_manager_t* manager = malloc(sizeof(model_manager_t));
    if (!manager) return NULL;
    
    memset(manager, 0, sizeof(model_manager_t));
    
    // 初始化互斥锁
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        free(manager);
        return NULL;
    }
    
    manager->capacity = 16;
    manager->models = calloc(manager->capacity, sizeof(ModelInstance*));
    if (!manager->models) {
        pthread_mutex_destroy(&manager->mutex);
        free(manager);
        return NULL;
    }
    
    return manager;
}

void model_manager_destroy(model_manager_t* manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 释放所有模型实例
    for (uint32_t i = 0; i < manager->count; i++) {
        if (manager->models[i]) {
            // 卸载模型
            if (manager->models[i]->engine) {
                infer_engine_destroy(manager->models[i]->engine);
            }
            free(manager->models[i]->model_id);
            free(manager->models[i]->model_path);
            free(manager->models[i]);
        }
    }
    
    free(manager->models);
    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    free(manager);
}

static ModelInstance* find_model_instance(model_manager_t* manager, const char* model_id) {
    for (uint32_t i = 0; i < manager->count; i++) {
        if (manager->models[i] && strcmp(manager->models[i]->model_id, model_id) == 0) {
            return manager->models[i];
        }
    }
    return NULL;
}

model_handle_t model_manager_load(model_manager_t* manager, const char* model_path, const model_config_t* config) {
    if (!manager || !model_path) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 使用默认配置或提供的配置
    model_config_t default_config = {0};
    const model_config_t* final_config = config ? config : &default_config;
    
    // 生成模型ID
    char model_id[256];
    if (final_config->model_id) {
        strncpy(model_id, final_config->model_id, sizeof(model_id) - 1);
    } else {
        snprintf(model_id, sizeof(model_id), "model_%u", manager->count);
    }
    
    // 检查模型是否已加载
    if (find_model_instance(manager, model_id)) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL; // 模型已存在
    }
    
    // 创建模型实例
    ModelInstance* instance = malloc(sizeof(ModelInstance));
    if (!instance) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    memset(instance, 0, sizeof(ModelInstance));
    instance->model_id = strdup(model_id);
    instance->model_path = strdup(model_path);
    instance->backend = final_config->backend;
    instance->max_instances = final_config->max_instances ? final_config->max_instances : 4;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&instance->mutex, NULL) != 0) {
        free(instance->model_id);
        free(instance->model_path);
        free(instance);
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    // 创建推理引擎
    infer_engine_config_t engine_config = {0};
    engine_config.backend = instance->backend;
    engine_config.num_threads = 4;
    
    instance->engine = infer_engine_create(instance->backend, &engine_config);
    if (!instance->engine) {
        free(instance->model_id);
        free(instance->model_path);
        free(instance);
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    // 加载模型
    if (infer_engine_load_model(instance->engine, model_path, NULL, 0) != 0) {
        infer_engine_destroy(instance->engine);
        free(instance->model_id);
        free(instance->model_path);
        free(instance);
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    // 添加到管理器
    if (manager->count >= manager->capacity) {
        uint32_t new_capacity = manager->capacity * 2;
        ModelInstance** new_models = realloc(manager->models, new_capacity * sizeof(ModelInstance*));
        if (!new_models) {
            infer_engine_destroy(instance->engine);
            free(instance->model_id);
            free(instance->model_path);
            free(instance);
            pthread_mutex_unlock(&manager->mutex);
            return NULL;
        }
        manager->models = new_models;
        manager->capacity = new_capacity;
    }
    
    manager->models[manager->count] = instance;
    manager->count++;
    
    // 创建模型句柄
    model_handle_t handle = malloc(sizeof(struct ModelHandle));
    if (!handle) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    handle->instance = instance;
    handle->manager = manager;
    
    pthread_mutex_unlock(&manager->mutex);
    
    printf("模型加载成功: %s -> %s\n", model_path, model_id);
    
    return handle;
}

int model_manager_unload(model_manager_t* manager, model_handle_t model) {
    if (!manager || !model) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 查找并移除模型实例
    for (uint32_t i = 0; i < manager->count; i++) {
        if (manager->models[i] == model->instance) {
            // 销毁推理引擎
            if (manager->models[i]->engine) {
                infer_engine_destroy(manager->models[i]->engine);
            }
            
            // 销毁互斥锁
            pthread_mutex_destroy(&manager->models[i]->mutex);
            
            // 释放内存
            free(manager->models[i]->model_id);
            free(manager->models[i]->model_path);
            free(manager->models[i]);
            
            // 移动后续元素
            if (i < manager->count - 1) {
                memmove(&manager->models[i], &manager->models[i + 1],
                        (manager->count - i - 1) * sizeof(ModelInstance*));
            }
            
            manager->count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    // 释放句柄
    free(model);
    
    return 0;
}

model_handle_t model_manager_get(model_manager_t* manager, const char* model_id) {
    if (!manager || !model_id) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    
    ModelInstance* instance = find_model_instance(manager, model_id);
    if (!instance) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    // 创建句柄
    model_handle_t handle = malloc(sizeof(struct ModelHandle));
    if (!handle) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    handle->instance = instance;
    handle->manager = manager;
    
    pthread_mutex_unlock(&manager->mutex);
    
    return handle;
}

int model_infer(model_handle_t model, const tensor_t* input_tensors, uint32_t input_count,
                tensor_t* output_tensors, uint32_t output_count) {
    if (!model || !model->instance || !model->instance->engine) return -1;
    
    // 记录开始时间
    clock_t start_time = clock();
    
    // 执行推理
    int ret = infer_engine_infer(model->instance->engine, input_tensors, input_count, output_tensors, output_count);
    
    // 如果推理成功，更新统计信息
    if (ret == 0) {
        clock_t end_time = clock();
        double latency_ms = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
        
        pthread_mutex_lock(&model->instance->mutex);
        model->instance->inference_count++;
        model->instance->total_latency += latency_ms;
        pthread_mutex_unlock(&model->instance->mutex);
    }
    
    return ret;
}

int model_infer_simple(model_handle_t model, const tensor_t* input, tensor_t* output) {
    return model_infer(model, input, 1, output, 1);
}

int model_manager_get_info(model_manager_t* manager, const char* model_id, model_info_t* info) {
    if (!manager || !model_id || !info) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    
    ModelInstance* instance = find_model_instance(manager, model_id);
    if (!instance) {
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // 填充信息 - 复制字符串以避免内存管理问题
    info->model_id = strdup(instance->model_id);
    info->version = strdup("1.0"); // 简化实现
    info->status = instance->engine ? MODEL_STATUS_LOADED : MODEL_STATUS_ERROR;
    info->instance_count = 1; // 简化实现
    info->memory_usage = 0; // 简化实现
    info->inference_count = instance->inference_count;
    info->avg_latency = instance->inference_count > 0 ? 
                       instance->total_latency / instance->inference_count : 0.0;
    
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

int model_manager_list(model_manager_t* manager, char** model_ids, uint32_t* count) {
    if (!manager || !model_ids || !count) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    
    uint32_t available = *count;
    uint32_t actual = manager->count < available ? manager->count : available;
    
    for (uint32_t i = 0; i < actual; i++) {
        model_ids[i] = strdup(manager->models[i]->model_id);
    }
    
    *count = actual;
    
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
} 