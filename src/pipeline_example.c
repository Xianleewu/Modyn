#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modyn.h"
#include "pipeline/modyn_pipeline.h"
#include "pipeline/dummy_node.h"

// 自定义预处理节点实现
static modyn_pipeline_node_status_e custom_preprocess_execute(
    modyn_pipeline_node_t *node,
    const modyn_tensor_data_t *inputs,
    size_t num_inputs,
    modyn_tensor_data_t **outputs,
    size_t *num_outputs,
    const modyn_pipeline_exec_context_t *context) {
    
    printf("  [Custom Preprocess] Processing %zu inputs\n", num_inputs);
    
    // 创建输出
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
    
    return MODYN_PIPELINE_NODE_SUCCESS;
}

static modyn_status_e custom_preprocess_validate(
    modyn_pipeline_node_t *node,
    const modyn_tensor_data_t *inputs,
    size_t num_inputs) {
    return MODYN_SUCCESS;
}

static void custom_preprocess_cleanup(
    modyn_pipeline_node_t *node,
    modyn_tensor_data_t *outputs,
    size_t num_outputs) {
    if (outputs) {
        for (size_t i = 0; i < num_outputs; ++i) {
            if (outputs[i].data) free(outputs[i].data);
        }
    }
}

static void custom_preprocess_destroy(modyn_pipeline_node_t *node) {
    if (node->private_data) {
        free(node->private_data);
    }
    free(node);
}

// 自定义预处理节点工厂函数
static modyn_pipeline_node_t* create_custom_preprocess_node(
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
    node->execute = custom_preprocess_execute;
    node->validate = custom_preprocess_validate;
    node->cleanup = custom_preprocess_cleanup;
    node->destroy = custom_preprocess_destroy;
    
    // 初始化其他字段
    node->cached_outputs = NULL;
    node->cached_num_outputs = 0;
    node->execution_count = 0;
    node->success_count = 0;
    node->error_count = 0;
    node->total_time_ms = 0;
    node->last_execution_time_ms = 0;
    node->private_data = NULL;
    
    return node;
}

// 演示复杂的pipeline构建
static void demonstrate_complex_pipeline() {
  printf("\n=== 复杂Pipeline演示（使用统一接口） ===\n");
  
  // 使用dummy节点类型注册
  printf("--- 注册Dummy节点类型 ---\n");
  
  modyn_status_e reg_status = modyn_register_dummy_node_types();
  if (reg_status != MODYN_SUCCESS) {
    printf("Failed to register dummy node types: %d\n", reg_status);
    return;
  }
  printf("✓ All dummy node types registered successfully\n");
  
  // 创建pipeline
  modyn_pipeline_handle_t pipeline = modyn_create_pipeline("unified_demo");
  if (!pipeline) {
    printf("Failed to create pipeline\n");
    return;
  }
  
  // 设置执行选项
  modyn_pipeline_set_execution_options(pipeline, 60000, 5, 1);
  
  printf("\n--- 添加节点到Pipeline ---\n");
  
  // 1. 添加预处理节点
  modyn_preprocess_config_t prep_config = {
    .input_modality = MODYN_MODALITY_IMAGE,
    .output_modality = MODYN_MODALITY_IMAGE,
    .resize_enabled = 1,
    .resize_width = 224,
    .resize_height = 224,
    .normalize_enabled = 1,
    .normalize_mean = {0.485f, 0.456f, 0.406f, 0.0f},
    .normalize_std = {0.229f, 0.224f, 0.225f, 1.0f},
    .color_space_convert = 0
  };
  
  modyn_status_e status = modyn_pipeline_add_node_by_type(pipeline, 
                                                         MODYN_PIPELINE_NODE_PREPROCESS,
                                                         "image_preprocess", 
                                                         &prep_config, 
                                                         sizeof(prep_config));
  if (status != MODYN_SUCCESS) {
    printf("Failed to add preprocess node: %d\n", status);
    return;
  }
  printf("✓ Added preprocess node: image_preprocess\n");
    
    // 2. 添加模型推理节点（使用dummy模型）
    modyn_model_handle_t dummy_model = (modyn_model_handle_t)0x1234; // 模拟模型句柄
    status = modyn_pipeline_add_node_by_type(pipeline, 
                                           MODYN_PIPELINE_NODE_MODEL,
                                           "classification_model", 
                                           &dummy_model, 
                                           sizeof(dummy_model));
    if (status != MODYN_SUCCESS) {
        printf("Failed to add model node: %d\n", status);
        return;
    }
    printf("✓ Added model node: classification_model\n");
    
    // 3. 添加条件节点
    modyn_conditional_config_t cond_config = {
        .condition_type = 0, // 阈值类型
        .threshold = 0.5f,
        .expression = "",
        .condition_func = NULL,
        .true_branch = 0,  // 条件为真时的分支
        .false_branch = 1  // 条件为假时的分支
    };
    
    status = modyn_pipeline_add_node_by_type(pipeline, 
                                           MODYN_PIPELINE_NODE_CONDITIONAL,
                                           "confidence_check", 
                                           &cond_config, 
                                           sizeof(cond_config));
    if (status != MODYN_SUCCESS) {
        printf("Failed to add conditional node: %d\n", status);
        return;
    }
    printf("✓ Added conditional node: confidence_check\n");
    
    // 4. 添加后处理节点
    modyn_postprocess_config_t post_config = {
        .input_modality = MODYN_MODALITY_IMAGE,
        .output_modality = MODYN_MODALITY_TEXT,
        .confidence_threshold = 0.5,
        .nms_enabled = 1,
        .nms_threshold = 0.3f,
        .max_detections = 10,
        .format_output = 1 // JSON格式
    };
    
    status = modyn_pipeline_add_node_by_type(pipeline, 
                                           MODYN_PIPELINE_NODE_POSTPROCESS,
                                           "result_format", 
                                           &post_config, 
                                           sizeof(post_config));
    if (status != MODYN_SUCCESS) {
        printf("Failed to add postprocess node: %d\n", status);
        return;
    }
    printf("✓ Added postprocess node: result_format\n");
    
    // 5. 添加循环节点
    modyn_loop_config_t loop_config = {
        .loop_type = 0, // 固定次数
        .max_iterations = 3,
        .condition_expr = "",
        .break_on_error = 1,
        .continue_on_skip = 0
    };
    
    status = modyn_pipeline_add_node_by_type(pipeline, 
                                           MODYN_PIPELINE_NODE_LOOP,
                                           "retry_loop", 
                                           &loop_config, 
                                           sizeof(loop_config));
    if (status != MODYN_SUCCESS) {
        printf("Failed to add loop node: %d\n", status);
        return;
    }
    printf("✓ Added loop node: retry_loop\n");
    
    // 6. 连接节点
    printf("\n--- 连接节点 ---\n");
    
    status = modyn_pipeline_connect_nodes(pipeline, "image_preprocess", 0, "classification_model", 0);
    printf("✓ Connected: image_preprocess[0] -> classification_model[0]\n");
    
    status = modyn_pipeline_connect_nodes(pipeline, "classification_model", 0, "confidence_check", 0);
    printf("✓ Connected: classification_model[0] -> confidence_check[0]\n");
    
    status = modyn_pipeline_connect_nodes(pipeline, "confidence_check", 0, "result_format", 0);
    printf("✓ Connected: confidence_check[0] -> result_format[0]\n");
    
    status = modyn_pipeline_connect_nodes(pipeline, "result_format", 0, "retry_loop", 0);
    printf("✓ Connected: result_format[0] -> retry_loop[0]\n");
    
    // 7. 查询拓扑结构
    printf("\n--- Pipeline拓扑结构 ---\n");
    modyn_pipeline_topology_t topology;
    status = modyn_query_pipeline_topology(pipeline, &topology);
    if (status == MODYN_SUCCESS) {
        printf("Pipeline: %s\n", topology.name);
        printf("Nodes: %zu\n", topology.num_nodes);
        printf("Edges: %zu\n", topology.num_edges);
        
        printf("\nNodes:\n");
        for (size_t i = 0; i < topology.num_nodes; ++i) {
            printf("  [%zu] %s (model: %p, inputs: %zu, outputs: %zu, source: %s, sink: %s)\n",
                   i, topology.nodes[i].name, topology.nodes[i].model_handle,
                   topology.nodes[i].num_inputs, topology.nodes[i].num_outputs,
                   topology.nodes[i].is_source ? "yes" : "no",
                   topology.nodes[i].is_sink ? "yes" : "no");
        }
        
        printf("\nEdges:\n");
        for (size_t i = 0; i < topology.num_edges; ++i) {
            printf("  [%zu] %s[%zu] -> %s[%zu]\n",
                   i, topology.edges[i].src_node, topology.edges[i].src_output_idx,
                   topology.edges[i].dst_node, topology.edges[i].dst_input_idx);
        }
        
        modyn_free_pipeline_topology(&topology);
    }
    
    // 8. 获取执行统计
    printf("\n--- 执行统计 ---\n");
    size_t total_nodes, executed_nodes, skipped_nodes, error_nodes;
    int total_time_ms;
    
    status = modyn_pipeline_get_execution_stats(pipeline, &total_nodes, &executed_nodes, 
                                               &skipped_nodes, &error_nodes, &total_time_ms);
    if (status == MODYN_SUCCESS) {
        printf("Total nodes: %zu\n", total_nodes);
        printf("Executed nodes: %zu\n", executed_nodes);
        printf("Skipped nodes: %zu\n", skipped_nodes);
        printf("Error nodes: %zu\n", error_nodes);
        printf("Total time: %d ms\n", total_time_ms);
    }
    
    printf("\n=== 复杂Pipeline演示完成 ===\n");
}

// 演示自定义节点类型注册
static void demonstrate_custom_node_registration() {
    printf("\n=== 自定义节点类型注册演示 ===\n");
    
    // 创建使用dummy节点的pipeline
    modyn_pipeline_handle_t pipeline = modyn_create_pipeline("custom_demo");
    if (!pipeline) {
        printf("Failed to create pipeline\n");
        return;
    }
    
    // 添加dummy预处理节点
    modyn_status_e status = modyn_pipeline_add_node_by_type(pipeline, 
                                           MODYN_PIPELINE_NODE_PREPROCESS,
                                           "dummy_image_preprocess", 
                                           NULL, 
                                           0);
    if (status == MODYN_SUCCESS) {
        printf("✓ Added dummy preprocess node: dummy_image_preprocess\n");
    } else {
        printf("Failed to add dummy preprocess node: %d\n", status);
    }
    
    printf("=== 自定义节点类型注册演示完成 ===\n");
}

// 主函数
int main() {
    printf("=== Modyn Pipeline 统一接口演示 ===\n");
    
    // 初始化框架
    modyn_status_e status = modyn_initialize(NULL);
    if (status != MODYN_SUCCESS) {
        printf("Failed to initialize framework: %d\n", status);
        return -1;
    }
    printf("✓ Framework initialized\n");
    
    // 演示复杂pipeline
    demonstrate_complex_pipeline();
    
    // 演示自定义节点类型注册
    demonstrate_custom_node_registration();
    
    // 关闭框架
    modyn_shutdown();
    printf("✓ Framework shutdown\n");
    
    printf("\n=== 演示完成 ===\n");
    return 0;
}
