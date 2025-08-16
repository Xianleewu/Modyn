#include "pipeline/pipeline_manager.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/**
 * @brief 管道节点结构
 */
struct PipelineNode {
    char* node_id;
    PipelineNodeType type;
    ModelHandle model;
    NodeProcessFunc process_func;
    void* context;
    uint32_t input_count;
    uint32_t output_count;
    Tensor* input_buffers;
    Tensor* output_buffers;
    struct PipelineNode* next;
};

// PipelineConnection已在头文件中定义，这里不需要重复定义

/**
 * @brief 管道结构
 */
struct Pipeline {
    char* pipeline_id;
    PipelineExecMode exec_mode;
    uint32_t max_parallel;
    bool enable_cache;
    struct PipelineNode* nodes;
    PipelineConnection* connections;
    uint32_t node_count;
    uint32_t connection_count;
    pthread_mutex_t mutex;
    struct Pipeline* next;  // 用于管理器链表
};

/**
 * @brief 管道管理器结构
 */
struct PipelineManager {
    struct Pipeline* pipelines;
    uint32_t pipeline_count;
    pthread_mutex_t mutex;
};

PipelineManager pipeline_manager_create(void) {
    PipelineManager manager = malloc(sizeof(struct PipelineManager));
    if (!manager) {
        LOG_ERROR("Failed to allocate pipeline manager");
        return NULL;
    }
    
    manager->pipelines = NULL;
    manager->pipeline_count = 0;
    
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize pipeline manager mutex");
        free(manager);
        return NULL;
    }
    
    LOG_INFO("管道管理器创建成功");
    return manager;
}

void pipeline_manager_destroy(PipelineManager manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 销毁所有管道
    struct Pipeline* current = manager->pipelines;
    while (current) {
        struct Pipeline* next = current->next;
        pipeline_destroy(current);
        current = next;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    
    free(manager);
    LOG_INFO("管道管理器销毁完成");
}

Pipeline pipeline_create(PipelineManager manager, const PipelineConfig* config) {
    if (!manager || !config) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    
    Pipeline pipeline = malloc(sizeof(struct Pipeline));
    if (!pipeline) {
        LOG_ERROR("Failed to allocate pipeline");
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    pipeline->pipeline_id = strdup(config->pipeline_id);
    pipeline->exec_mode = config->exec_mode;
    pipeline->max_parallel = config->max_parallel;
    pipeline->enable_cache = config->enable_cache;
    pipeline->nodes = NULL;
    pipeline->connections = NULL;
    pipeline->node_count = 0;
    pipeline->connection_count = 0;
    
    if (pthread_mutex_init(&pipeline->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize pipeline mutex");
        free(pipeline->pipeline_id);
        free(pipeline);
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    // 添加到管理器链表
    pipeline->next = manager->pipelines;
    manager->pipelines = pipeline;
    manager->pipeline_count++;
    
    pthread_mutex_unlock(&manager->mutex);
    
    LOG_INFO("管道创建成功: %s", config->pipeline_id);
    return pipeline;
}

void pipeline_destroy(Pipeline pipeline) {
    if (!pipeline) return;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    // 销毁所有节点
    struct PipelineNode* node = pipeline->nodes;
    while (node) {
        struct PipelineNode* next_node = node->next;
        
        // 释放节点资源
        free(node->node_id);
        if (node->input_buffers) {
            for (uint32_t i = 0; i < node->input_count; i++) {
                tensor_free(&node->input_buffers[i]);
            }
            free(node->input_buffers);
        }
        if (node->output_buffers) {
            for (uint32_t i = 0; i < node->output_count; i++) {
                tensor_free(&node->output_buffers[i]);
            }
            free(node->output_buffers);
        }
        
        free(node);
        node = next_node;
    }
    
    // 释放管道资源
    free(pipeline->pipeline_id);
    
    pthread_mutex_unlock(&pipeline->mutex);
    pthread_mutex_destroy(&pipeline->mutex);
    
    free(pipeline);
    LOG_INFO("管道销毁完成");
}

static struct PipelineNode* find_node(Pipeline pipeline, const char* node_id) {
    struct PipelineNode* node = pipeline->nodes;
    while (node) {
        if (strcmp(node->node_id, node_id) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

PipelineNode pipeline_add_node(Pipeline pipeline, const PipelineNodeConfig* config) {
    if (!pipeline || !config) return NULL;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    // 检查节点是否已存在
    if (find_node(pipeline, config->node_id)) {
        LOG_ERROR("节点 %s 已存在", config->node_id);
        pthread_mutex_unlock(&pipeline->mutex);
        return NULL;
    }
    
    struct PipelineNode* node = malloc(sizeof(struct PipelineNode));
    if (!node) {
        LOG_ERROR("Failed to allocate pipeline node");
        pthread_mutex_unlock(&pipeline->mutex);
        return NULL;
    }
    
    node->node_id = strdup(config->node_id);
    node->type = config->type;
    node->model = config->model;
    node->process_func = config->process_func;
    node->context = config->context;
    node->input_count = config->input_count;
    node->output_count = config->output_count;
    node->input_buffers = NULL;
    node->output_buffers = NULL;
    node->next = NULL;
    
    // 分配输入缓冲区
    if (config->input_count > 0) {
        node->input_buffers = calloc(config->input_count, sizeof(Tensor));
        if (!node->input_buffers) {
            LOG_ERROR("Failed to allocate input buffers");
            free(node->node_id);
            free(node);
            pthread_mutex_unlock(&pipeline->mutex);
            return NULL;
        }
    }
    
    // 分配输出缓冲区
    if (config->output_count > 0) {
        node->output_buffers = calloc(config->output_count, sizeof(Tensor));
        if (!node->output_buffers) {
            LOG_ERROR("Failed to allocate output buffers");
            free(node->input_buffers);
            free(node->node_id);
            free(node);
            pthread_mutex_unlock(&pipeline->mutex);
            return NULL;
        }
    }
    
    // 添加到管道节点链表
    node->next = pipeline->nodes;
    pipeline->nodes = node;
    pipeline->node_count++;
    
    pthread_mutex_unlock(&pipeline->mutex);
    
    LOG_INFO("节点添加成功: %s", config->node_id);
    return node;
}

int pipeline_connect_nodes(Pipeline pipeline, const PipelineConnection* connection) {
    if (!pipeline || !connection) return -1;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    // 查找源节点和目标节点
    struct PipelineNode* from_node = find_node(pipeline, connection->from_node);
    struct PipelineNode* to_node = find_node(pipeline, connection->to_node);
    
    if (!from_node) {
        LOG_ERROR("源节点 %s 不存在", connection->from_node);
        pthread_mutex_unlock(&pipeline->mutex);
        return -1;
    }
    
    if (!to_node) {
        LOG_ERROR("目标节点 %s 不存在", connection->to_node);
        pthread_mutex_unlock(&pipeline->mutex);
        return -1;
    }
    
    // 验证连接有效性
    if (connection->from_output >= from_node->output_count) {
        LOG_ERROR("源节点 %s 输出索引 %u 超出范围", connection->from_node, connection->from_output);
        pthread_mutex_unlock(&pipeline->mutex);
        return -1;
    }
    
    if (connection->to_input >= to_node->input_count) {
        LOG_ERROR("目标节点 %s 输入索引 %u 超出范围", connection->to_node, connection->to_input);
        pthread_mutex_unlock(&pipeline->mutex);
        return -1;
    }
    
    // 简化实现：这里应该建立实际的连接关系
    // 实际实现中需要维护连接图并在执行时传递数据
    
    pipeline->connection_count++;
    
    pthread_mutex_unlock(&pipeline->mutex);
    
    LOG_INFO("节点连接成功: %s[%u] -> %s[%u]", 
             connection->from_node, connection->from_output,
             connection->to_node, connection->to_input);
    
    return 0;
}

static int execute_node(struct PipelineNode* node) {
    if (!node) return -1;
    
    LOG_TRACE("执行节点: %s", node->node_id);
    
    switch (node->type) {
        case PIPELINE_NODE_MODEL:
            if (node->model) {
                return model_infer(node->model, node->input_buffers, node->input_count,
                                 node->output_buffers, node->output_count);
            }
            break;
        
        case PIPELINE_NODE_CUSTOM:
            if (node->process_func) {
                return node->process_func(node->input_buffers, node->input_count,
                                        node->output_buffers, node->output_count,
                                        node->context);
            }
            break;
        
        default:
            LOG_WARN("未支持的节点类型: %d", node->type);
            return -1;
    }
    
    LOG_ERROR("节点 %s 缺少执行函数", node->node_id);
    return -1;
}

int pipeline_execute(Pipeline pipeline, const Tensor* inputs, uint32_t input_count,
                     Tensor* outputs, uint32_t output_count) {
    if (!pipeline) return -1;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    // 简化实现：顺序执行所有节点
    struct PipelineNode* node = pipeline->nodes;
    while (node) {
        int result = execute_node(node);
        if (result != 0) {
            LOG_ERROR("节点 %s 执行失败", node->node_id);
            pthread_mutex_unlock(&pipeline->mutex);
            return result;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&pipeline->mutex);
    
    LOG_INFO("管道执行完成");
    return 0;
    
    // 避免未使用参数警告
    (void)inputs;
    (void)input_count;
    (void)outputs;
    (void)output_count;
}

int pipeline_get_info(Pipeline pipeline, uint32_t* node_count, uint32_t* connection_count) {
    if (!pipeline || !node_count || !connection_count) return -1;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    *node_count = pipeline->node_count;
    *connection_count = pipeline->connection_count;
    
    pthread_mutex_unlock(&pipeline->mutex);
    
    return 0;
}

int pipeline_validate(Pipeline pipeline) {
    if (!pipeline) return -1;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    // 简化实现：检查是否有节点
    bool has_nodes = (pipeline->nodes != NULL);
    
    pthread_mutex_unlock(&pipeline->mutex);
    
    return has_nodes ? 0 : -1;
} 