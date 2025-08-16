#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "dummy_node.h"

// ======================== Dummy Node Private Data ======================== //

typedef struct dummy_node_private_s {
  char description[128];           // 节点描述
  int processing_delay_ms;         // 模拟处理延迟
  int success_rate_percent;        // 成功率百分比
  int max_retries;                 // 最大重试次数
  void *user_config;               // 用户配置数据
} dummy_node_private_t;

// ======================== Dummy Node Execution Functions ======================== //

// 通用的dummy节点执行函数
static modyn_pipeline_node_status_e dummy_node_execute(
    modyn_pipeline_node_t *node,
    const modyn_tensor_data_t *inputs,
    size_t num_inputs,
    modyn_tensor_data_t **outputs,
    size_t *num_outputs,
    const modyn_pipeline_exec_context_t *context) {
    
    dummy_node_private_t *priv = (dummy_node_private_t*)node->private_data;
    if (!priv) return MODYN_PIPELINE_NODE_ERROR;
    
    printf("  [%s] Executing dummy node: %s\n", priv->description, node->name);
    printf("    Inputs: %zu, Processing delay: %dms\n", num_inputs, priv->processing_delay_ms);
    
    // 模拟处理延迟
    if (priv->processing_delay_ms > 0) {
        struct timespec ts = {
            .tv_sec = priv->processing_delay_ms / 1000,
            .tv_nsec = (priv->processing_delay_ms % 1000) * 1000000
        };
        nanosleep(&ts, NULL);
    }
    
    // 模拟成功率
    int rand_val = rand() % 100;
    if (rand_val >= priv->success_rate_percent) {
        printf("    [%s] Execution failed (random failure)\n", node->name);
        return MODYN_PIPELINE_NODE_ERROR;
    }
    
    // 创建输出（简单复制输入）
    *outputs = (modyn_tensor_data_t*)malloc(num_inputs * sizeof(modyn_tensor_data_t));
    if (!*outputs) return MODYN_PIPELINE_NODE_ERROR;
    
    *num_outputs = num_inputs;
    for (size_t i = 0; i < num_inputs; ++i) {
        (*outputs)[i] = inputs[i];
        if (inputs[i].data) {
            (*outputs)[i].data = malloc(inputs[i].size);
            if ((*outputs)[i].data) {
                memcpy((*outputs)[i].data, inputs[i].data, inputs[i].size);
            }
        }
    }
    
    printf("    [%s] Execution successful, outputs: %zu\n", node->name, *num_outputs);
    return MODYN_PIPELINE_NODE_SUCCESS;
}

// 通用的dummy节点验证函数
static modyn_status_e dummy_node_validate(
    modyn_pipeline_node_t *node,
    const modyn_tensor_data_t *inputs,
    size_t num_inputs) {
    
    dummy_node_private_t *priv = (dummy_node_private_t*)node->private_data;
    if (!priv) return MODYN_ERROR_INVALID_ARGUMENT;
    
    printf("  [%s] Validating dummy node: %s\n", priv->description, node->name);
    
    // 简单的验证：检查输入数量
    if (num_inputs == 0) {
        printf("    [%s] Validation failed: no inputs\n", node->name);
        return MODYN_ERROR_INVALID_ARGUMENT;
    }
    
    printf("    [%s] Validation successful\n", node->name);
    return MODYN_SUCCESS;
}

// 通用的dummy节点清理函数
static void dummy_node_cleanup(
    modyn_pipeline_node_t *node,
    modyn_tensor_data_t *outputs,
    size_t num_outputs) {
    
    dummy_node_private_t *priv = (dummy_node_private_t*)node->private_data;
    if (!priv) return;
    
    printf("  [%s] Cleaning up dummy node: %s\n", priv->description, node->name);
    
    if (outputs) {
        for (size_t i = 0; i < num_outputs; ++i) {
            if (outputs[i].data) {
                free(outputs[i].data);
            }
        }
    }
}

// 通用的dummy节点销毁函数
static void dummy_node_destroy(modyn_pipeline_node_t *node) {
    if (!node) return;
    
    dummy_node_private_t *priv = (dummy_node_private_t*)node->private_data;
    if (priv) {
        if (priv->user_config) {
            free(priv->user_config);
        }
        free(priv);
    }
    
    free(node);
}

// ======================== Dummy Node Factory Functions ======================== //

// 创建dummy预处理节点
modyn_pipeline_node_t* modyn_create_dummy_preprocess_node(
    const char *name,
    const void *config_data,
    size_t config_size) {
    
    modyn_pipeline_node_t *node = (modyn_pipeline_node_t*)calloc(1, sizeof(modyn_pipeline_node_t));
    if (!node) return NULL;
    
    // 设置基本信息
    strncpy(node->name, name, sizeof(node->name)-1);
    node->type = MODYN_PIPELINE_NODE_PREPROCESS;
    
    // 设置配置
    node->config.enabled = 1;
    node->config.timeout_ms = 30000;
    node->config.retry_count = 3;
    node->config.priority = 0;
    node->config.config_data = NULL;
    node->config.config_size = 0;
    
    // 复制配置数据
    if (config_data && config_size > 0) {
        node->config.config_data = malloc(config_size);
        if (node->config.config_data) {
            memcpy(node->config.config_data, config_data, config_size);
            node->config.config_size = config_size;
        }
    }
    
    // 设置执行接口
    node->execute = dummy_node_execute;
    node->validate = dummy_node_validate;
    node->cleanup = dummy_node_cleanup;
    node->destroy = dummy_node_destroy;
    
    // 初始化其他字段
    node->cached_outputs = NULL;
    node->cached_num_outputs = 0;
    node->execution_count = 0;
    node->success_count = 0;
    node->error_count = 0;
    node->total_time_ms = 0;
    node->last_execution_time_ms = 0;
    
    // 创建私有数据
    dummy_node_private_t *priv = (dummy_node_private_t*)calloc(1, sizeof(dummy_node_private_t));
    if (priv) {
        strcpy(priv->description, "Dummy Preprocess");
        priv->processing_delay_ms = 50;  // 50ms延迟
        priv->success_rate_percent = 95; // 95%成功率
        priv->max_retries = 3;
        priv->user_config = NULL;
        node->private_data = priv;
    }
    
    return node;
}

// 创建dummy后处理节点
modyn_pipeline_node_t* modyn_create_dummy_postprocess_node(
    const char *name,
    const void *config_data,
    size_t config_size) {
    
    modyn_pipeline_node_t *node = (modyn_pipeline_node_t*)calloc(1, sizeof(modyn_pipeline_node_t));
    if (!node) return NULL;
    
    // 设置基本信息
    strncpy(node->name, name, sizeof(node->name)-1);
    node->type = MODYN_PIPELINE_NODE_POSTPROCESS;
    
    // 设置配置
    node->config.enabled = 1;
    node->config.timeout_ms = 30000;
    node->config.retry_count = 3;
    node->config.priority = 0;
    node->config.config_data = NULL;
    node->config.config_size = 0;
    
    // 复制配置数据
    if (config_data && config_size > 0) {
        node->config.config_data = malloc(config_size);
        if (node->config.config_data) {
            memcpy(node->config.config_data, config_data, config_size);
            node->config.config_size = config_size;
        }
    }
    
    // 设置执行接口
    node->execute = dummy_node_execute;
    node->validate = dummy_node_validate;
    node->cleanup = dummy_node_cleanup;
    node->destroy = dummy_node_destroy;
    
    // 初始化其他字段
    node->cached_outputs = NULL;
    node->cached_num_outputs = 0;
    node->execution_count = 0;
    node->success_count = 0;
    node->error_count = 0;
    node->total_time_ms = 0;
    node->last_execution_time_ms = 0;
    
    // 创建私有数据
    dummy_node_private_t *priv = (dummy_node_private_t*)calloc(1, sizeof(dummy_node_private_t));
    if (priv) {
        strcpy(priv->description, "Dummy Postprocess");
        priv->processing_delay_ms = 30;  // 30ms延迟
        priv->success_rate_percent = 98; // 98%成功率
        priv->max_retries = 2;
        priv->user_config = NULL;
        node->private_data = priv;
    }
    
    return node;
}

// 创建dummy条件节点
modyn_pipeline_node_t* modyn_create_dummy_conditional_node(
    const char *name,
    const void *config_data,
    size_t config_size) {
    
    modyn_pipeline_node_t *node = (modyn_pipeline_node_t*)calloc(1, sizeof(modyn_pipeline_node_t));
    if (!node) return NULL;
    
    // 设置基本信息
    strncpy(node->name, name, sizeof(node->name)-1);
    node->type = MODYN_PIPELINE_NODE_CONDITIONAL;
    
    // 设置配置
    node->config.enabled = 1;
    node->config.timeout_ms = 10000;
    node->config.retry_count = 1;
    node->config.priority = 5;
    node->config.config_data = NULL;
    node->config.config_size = 0;
    
    // 复制配置数据
    if (config_data && config_size > 0) {
        node->config.config_data = malloc(config_size);
        if (node->config.config_data) {
            memcpy(node->config.config_data, config_data, config_size);
            node->config.config_size = config_size;
        }
    }
    
    // 设置执行接口
    node->execute = dummy_node_execute;
    node->validate = dummy_node_validate;
    node->cleanup = dummy_node_cleanup;
    node->destroy = dummy_node_destroy;
    
    // 初始化其他字段
    node->cached_outputs = NULL;
    node->cached_num_outputs = 0;
    node->execution_count = 0;
    node->success_count = 0;
    node->error_count = 0;
    node->total_time_ms = 0;
    node->last_execution_time_ms = 0;
    
    // 创建私有数据
    dummy_node_private_t *priv = (dummy_node_private_t*)calloc(1, sizeof(dummy_node_private_t));
    if (priv) {
        strcpy(priv->description, "Dummy Conditional");
        priv->processing_delay_ms = 10;  // 10ms延迟
        priv->success_rate_percent = 99; // 99%成功率
        priv->max_retries = 1;
        priv->user_config = NULL;
        node->private_data = priv;
    }
    
    return node;
}

// 创建dummy循环节点
modyn_pipeline_node_t* modyn_create_dummy_loop_node(
    const char *name,
    const void *config_data,
    size_t config_size) {
    
    modyn_pipeline_node_t *node = (modyn_pipeline_node_t*)calloc(1, sizeof(modyn_pipeline_node_t));
    if (!node) return NULL;
    
    // 设置基本信息
    strncpy(node->name, name, sizeof(node->name)-1);
    node->type = MODYN_PIPELINE_NODE_LOOP;
    
    // 设置配置
    node->config.enabled = 1;
    node->config.timeout_ms = 60000;
    node->config.retry_count = 5;
    node->config.priority = 3;
    node->config.config_data = NULL;
    node->config.config_size = 0;
    
    // 复制配置数据
    if (config_data && config_size > 0) {
        node->config.config_data = malloc(config_size);
        if (node->config.config_data) {
            memcpy(node->config.config_data, config_data, config_size);
            node->config.config_size = config_size;
        }
    }
    
    // 设置执行接口
    node->execute = dummy_node_execute;
    node->validate = dummy_node_validate;
    node->cleanup = dummy_node_cleanup;
    node->destroy = dummy_node_destroy;
    
    // 初始化其他字段
    node->cached_outputs = NULL;
    node->cached_num_outputs = 0;
    node->execution_count = 0;
    node->success_count = 0;
    node->error_count = 0;
    node->total_time_ms = 0;
    node->last_execution_time_ms = 0;
    
    // 创建私有数据
    dummy_node_private_t *priv = (dummy_node_private_t*)calloc(1, sizeof(dummy_node_private_t));
    if (priv) {
        strcpy(priv->description, "Dummy Loop");
        priv->processing_delay_ms = 100; // 100ms延迟
        priv->success_rate_percent = 90; // 90%成功率
        priv->max_retries = 5;
        priv->user_config = NULL;
        node->private_data = priv;
    }
    
    return node;
}

// 创建dummy模型节点
modyn_pipeline_node_t* modyn_create_dummy_model_node(
    const char *name,
    const void *config_data,
    size_t config_size) {
    
    modyn_pipeline_node_t *node = (modyn_pipeline_node_t*)calloc(1, sizeof(modyn_pipeline_node_t));
    if (!node) return NULL;
    
    // 设置基本信息
    strncpy(node->name, name, sizeof(node->name)-1);
    node->type = MODYN_PIPELINE_NODE_MODEL;
    
    // 设置配置
    node->config.enabled = 1;
    node->config.timeout_ms = 120000;
    node->config.retry_count = 2;
    node->config.priority = 1;
    node->config.config_data = NULL;
    node->config.config_size = 0;
    
    // 复制配置数据
    if (config_data && config_size > 0) {
        node->config.config_data = malloc(config_size);
        if (node->config.config_data) {
            memcpy(node->config.config_data, config_data, config_size);
            node->config.config_size = config_size;
        }
    }
    
    // 设置执行接口
    node->execute = dummy_node_execute;
    node->validate = dummy_node_validate;
    node->cleanup = dummy_node_cleanup;
    node->destroy = dummy_node_destroy;
    
    // 初始化其他字段
    node->cached_outputs = NULL;
    node->cached_num_outputs = 0;
    node->execution_count = 0;
    node->success_count = 0;
    node->error_count = 0;
    node->total_time_ms = 0;
    node->last_execution_time_ms = 0;
    
    // 创建私有数据
    dummy_node_private_t *priv = (dummy_node_private_t*)calloc(1, sizeof(dummy_node_private_t));
    if (priv) {
        strcpy(priv->description, "Dummy Model");
        priv->processing_delay_ms = 200; // 200ms延迟
        priv->success_rate_percent = 85; // 85%成功率
        priv->max_retries = 2;
        priv->user_config = NULL;
        node->private_data = priv;
    }
    
    return node;
}

// ======================== Dummy Node Registration ======================== //

// 注册所有dummy节点类型
modyn_status_e modyn_register_dummy_node_types(void) {
    printf("Registering dummy node types...\n");
    
    modyn_status_e status;
    
    // 注册预处理节点类型
    status = modyn_register_pipeline_node_type(MODYN_PIPELINE_NODE_PREPROCESS,
                                              "dummy_preprocess",
                                              modyn_create_dummy_preprocess_node);
    if (status != MODYN_SUCCESS) {
        printf("Failed to register dummy preprocess node type: %d\n", status);
        return status;
    }
    printf("✓ Registered dummy preprocess node type\n");
    
    // 注册后处理节点类型
    status = modyn_register_pipeline_node_type(MODYN_PIPELINE_NODE_POSTPROCESS,
                                              "dummy_postprocess",
                                              modyn_create_dummy_postprocess_node);
    if (status != MODYN_SUCCESS) {
        printf("Failed to register dummy postprocess node type: %d\n", status);
        return status;
    }
    printf("✓ Registered dummy postprocess node type\n");
    
    // 注册条件节点类型
    status = modyn_register_pipeline_node_type(MODYN_PIPELINE_NODE_CONDITIONAL,
                                              "dummy_conditional",
                                              modyn_create_dummy_conditional_node);
    if (status != MODYN_SUCCESS) {
        printf("Failed to register dummy conditional node type: %d\n", status);
        return status;
    }
    printf("✓ Registered dummy conditional node type\n");
    
    // 注册循环节点类型
    status = modyn_register_pipeline_node_type(MODYN_PIPELINE_NODE_LOOP,
                                              "dummy_loop",
                                              modyn_create_dummy_loop_node);
    if (status != MODYN_SUCCESS) {
        printf("Failed to register dummy loop node type: %d\n", status);
        return status;
    }
    printf("✓ Registered dummy loop node type\n");
    
    // 注册模型节点类型
    status = modyn_register_pipeline_node_type(MODYN_PIPELINE_NODE_MODEL,
                                              "dummy_model",
                                              modyn_create_dummy_model_node);
    if (status != MODYN_SUCCESS) {
        printf("Failed to register dummy model node type: %d\n", status);
        return status;
    }
    printf("✓ Registered dummy model node type\n");
    
    printf("All dummy node types registered successfully!\n");
    return MODYN_SUCCESS;
}

// 注销所有dummy节点类型
modyn_status_e modyn_unregister_dummy_node_types(void) {
    printf("Unregistering dummy node types...\n");
    
    modyn_status_e status;
    
    // 注销所有类型
    status = modyn_unregister_pipeline_node_type(MODYN_PIPELINE_NODE_PREPROCESS);
    if (status == MODYN_SUCCESS) printf("✓ Unregistered dummy preprocess node type\n");
    
    status = modyn_unregister_pipeline_node_type(MODYN_PIPELINE_NODE_POSTPROCESS);
    if (status == MODYN_SUCCESS) printf("✓ Unregistered dummy postprocess node type\n");
    
    status = modyn_unregister_pipeline_node_type(MODYN_PIPELINE_NODE_CONDITIONAL);
    if (status == MODYN_SUCCESS) printf("✓ Unregistered dummy conditional node type\n");
    
    status = modyn_unregister_pipeline_node_type(MODYN_PIPELINE_NODE_LOOP);
    if (status == MODYN_SUCCESS) printf("✓ Unregistered dummy loop node type\n");
    
    status = modyn_unregister_pipeline_node_type(MODYN_PIPELINE_NODE_MODEL);
    if (status == MODYN_SUCCESS) printf("✓ Unregistered dummy model node type\n");
    
    printf("All dummy node types unregistered!\n");
    return MODYN_SUCCESS;
}
