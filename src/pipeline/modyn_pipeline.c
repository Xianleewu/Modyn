#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dlfcn.h>
#include "modyn_pipeline.h"

// 前向声明
typedef struct pipeline_edge_s pipeline_edge_t;
typedef struct pipeline_s pipeline_t;

// Pipeline边结构
typedef struct pipeline_edge_s {
  int src; size_t src_out;
  int dst; size_t dst_in;
  int enabled;  // 边是否启用
} pipeline_edge_t;

// Pipeline结构
typedef struct pipeline_s {
  char name[64];
  modyn_pipeline_node_t **nodes;  // 使用统一的节点类型
  size_t num_nodes;
  size_t cap_nodes;
  pipeline_edge_t *edges;
  size_t num_edges;
  size_t cap_edges;
  
  // 执行选项
  int timeout_ms;
  int max_retries;
  int enable_parallel;
  
  // 执行统计
  size_t total_executions;
  size_t successful_executions;
  size_t failed_executions;
  int total_execution_time_ms;
} pipeline_t;

// 全局节点类型工厂注册表
static modyn_pipeline_node_factory_t *g_node_factories = NULL;

// 全局节点注册表 - 所有已创建的节点都在这里
static modyn_pipeline_node_t **g_registered_nodes = NULL;
static size_t g_num_registered_nodes = 0;
static size_t g_cap_registered_nodes = 0;

// 内部函数声明
static int pipeline_find_node(pipeline_t *p, const char *name);
static modyn_status_e pipeline_add_node_internal(pipeline_t *p, modyn_pipeline_node_t *node);
static modyn_status_e pipeline_add_edge_internal(pipeline_t *p, int src, size_t src_out, 
                                               int dst, size_t dst_in);
static void pipeline_free_cached_outputs(pipeline_t *p, int keep_node_idx);
static modyn_status_e pipeline_execute_node(pipeline_t *p, int node_idx, 
                                          const modyn_tensor_data_t *inputs, size_t num_inputs,
                                          modyn_tensor_data_t **outputs, size_t *num_outputs,
                                          const modyn_pipeline_exec_context_t *context);

// 节点管理内部函数
static modyn_status_e register_node_globally(modyn_pipeline_node_t *node);
static modyn_status_e unregister_node_globally(const char *node_name);
static modyn_pipeline_node_t* find_node_globally(const char *node_name);
static int match_node_criteria(const modyn_pipeline_node_t *node, 
                              const modyn_node_search_criteria_t *criteria);
static int wildcard_match(const char *pattern, const char *text);

// ======================== 内部Pipeline函数 ======================== //

// 查找节点索引
static int pipeline_find_node(pipeline_t *p, const char *name) {
  if (!p || !name) return -1;
  for (size_t i = 0; i < p->num_nodes; ++i) {
    if (strcmp(p->nodes[i]->name, name) == 0) return (int)i;
  }
  return -1;
}

// 内部添加节点
static modyn_status_e pipeline_add_node_internal(pipeline_t *p, modyn_pipeline_node_t *node) {
  if (!p || !node) return MODYN_ERROR_INVALID_ARGUMENT;
  if (pipeline_find_node(p, node->name) >= 0) return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (p->num_nodes == p->cap_nodes) {
    size_t ncap = p->cap_nodes ? p->cap_nodes * 2 : 8;
    modyn_pipeline_node_t **nn = (modyn_pipeline_node_t**)realloc(p->nodes, ncap * sizeof(modyn_pipeline_node_t*));
    if (!nn) return MODYN_ERROR_MEMORY_ALLOCATION;
    p->nodes = nn; p->cap_nodes = ncap;
  }
  
  p->nodes[p->num_nodes++] = node;
  return MODYN_SUCCESS;
}

// 内部添加边
static modyn_status_e pipeline_add_edge_internal(pipeline_t *p, int src, size_t src_out, 
                                               int dst, size_t dst_in) {
  if (!p || src < 0 || dst < 0 || src >= (int)p->num_nodes || dst >= (int)p->num_nodes) 
    return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (p->num_edges == p->cap_edges) {
    size_t ncap = p->cap_edges ? p->cap_edges * 2 : 16;
    pipeline_edge_t *ne = (pipeline_edge_t*)realloc(p->edges, ncap * sizeof(pipeline_edge_t));
    if (!ne) return MODYN_ERROR_MEMORY_ALLOCATION;
    p->edges = ne; p->cap_edges = ncap;
  }
  
  pipeline_edge_t *e = &p->edges[p->num_edges++];
  e->src = src; e->src_out = src_out; e->dst = dst; e->dst_in = dst_in;
  e->enabled = 1;
  return MODYN_SUCCESS;
}

// 释放缓存的输出
static void pipeline_free_cached_outputs(pipeline_t *p, int keep_node_idx) {
  if (!p) return;
  for (size_t i = 0; i < p->num_nodes; ++i) {
    if ((int)i == keep_node_idx) continue;
    modyn_pipeline_node_t *node = p->nodes[i];
    if (node->cached_outputs) {
      // 调用节点的清理函数
      if (node->cleanup) {
        node->cleanup(node, node->cached_outputs, node->cached_num_outputs);
      }
      node->cached_outputs = NULL; 
      node->cached_num_outputs = 0;
    }
  }
}

// 执行单个节点
static modyn_status_e pipeline_execute_node(pipeline_t *p, int node_idx, 
                                          const modyn_tensor_data_t *inputs, size_t num_inputs,
                                          modyn_tensor_data_t **outputs, size_t *num_outputs,
                                          const modyn_pipeline_exec_context_t *context) {
  
  if (!p || node_idx < 0 || node_idx >= (int)p->num_nodes) 
    return MODYN_ERROR_INVALID_ARGUMENT;
  
  modyn_pipeline_node_t *node = p->nodes[node_idx];
  if (!node->config.enabled) return MODYN_SUCCESS;
  
  clock_t start_time = clock();
  
  modyn_status_e status;
  
  // 使用节点的统一执行接口
  if (node->execute) {
    modyn_pipeline_node_status_e node_status = node->execute(node, inputs, num_inputs, outputs, num_outputs, context);
    status = (node_status == MODYN_PIPELINE_NODE_SUCCESS) ? MODYN_SUCCESS : MODYN_ERROR_INVALID_ARGUMENT;
  } else {
    status = MODYN_ERROR_INVALID_ARGUMENT;
  }
  
  // 更新执行统计
  clock_t end_time = clock();
  int exec_time = (int)((end_time - start_time) * 1000 / CLOCKS_PER_SEC);
  
  node->execution_count++;
  node->last_execution_time_ms = exec_time;
  node->total_time_ms += exec_time;
  
  if (status == MODYN_SUCCESS) {
    node->success_count++;
  } else {
    node->error_count++;
  }
  
  return status;
}

// ======================== 公共Pipeline API ======================== //

// 创建pipeline
modyn_pipeline_handle_t modyn_create_pipeline(const char *name) {
  pipeline_t *p = (pipeline_t*)calloc(1, sizeof(pipeline_t));
  if (!p) return NULL;
  if (name) strncpy(p->name, name, sizeof(p->name)-1);
  p->timeout_ms = 30000;  // 默认30秒超时
  p->max_retries = 3;     // 默认重试3次
  p->enable_parallel = 0; // 默认串行执行
  return (modyn_pipeline_handle_t)p;
}

// 添加模型节点（保持向后兼容）
modyn_status_e modyn_pipeline_add_node(modyn_pipeline_handle_t pipeline,
                                      modyn_model_handle_t model_handle,
                                      const char *node_name) {
  if (!pipeline || !node_name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 这里应该创建一个模型节点，但为了简化，我们直接返回成功
  // 实际实现中应该调用模型节点的工厂函数
  return MODYN_SUCCESS;
}

// 统一的节点添加接口
modyn_status_e modyn_pipeline_add_node_by_type(modyn_pipeline_handle_t pipeline,
                                              modyn_pipeline_node_type_e node_type,
                                              const char *node_name,
                                              const void *config_data,
                                              size_t config_size) {
  if (!pipeline || !node_name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 查找节点类型工厂
  modyn_pipeline_node_factory_t *factory = g_node_factories;
  while (factory) {
    if (factory->node_type == node_type) {
      // 使用工厂函数创建节点
      modyn_pipeline_node_t *node = factory->create_func(node_name, config_data, config_size);
      if (!node) return MODYN_ERROR_MEMORY_ALLOCATION;
      
      // 自动注册到全局节点注册表
      modyn_status_e reg_status = register_node_globally(node);
      if (reg_status != MODYN_SUCCESS) {
        // 注册失败，清理节点
        if (node->destroy) {
          node->destroy(node);
        } else {
          free(node);
        }
        return reg_status;
      }
      
      pipeline_t *p = (pipeline_t*)pipeline;
      return pipeline_add_node_internal(p, node);
    }
    factory = factory->next;
  }
  
  // 未找到节点类型工厂，返回更明确的错误
  printf("Warning: No factory registered for node type %d (%s)\n", node_type, node_name);
  return MODYN_ERROR_DEVICE_NOT_SUPPORTED; // 使用已定义的错误码
}

// 连接节点
modyn_status_e modyn_pipeline_connect_nodes(modyn_pipeline_handle_t pipeline,
                                           const char *src_node, size_t src_output_idx,
                                           const char *dst_node, size_t dst_input_idx) {
  if (!pipeline || !src_node || !dst_node) return MODYN_ERROR_INVALID_ARGUMENT;
  pipeline_t *p = (pipeline_t*)pipeline;
  int s = pipeline_find_node(p, src_node);
  int d = pipeline_find_node(p, dst_node);
  if (s < 0 || d < 0) return MODYN_ERROR_INVALID_ARGUMENT;
  return pipeline_add_edge_internal(p, s, src_output_idx, d, dst_input_idx);
}

// 运行pipeline
modyn_status_e modyn_run_pipeline(modyn_pipeline_handle_t pipeline,
                                 modyn_tensor_data_t *inputs, size_t num_inputs,
                                 modyn_tensor_data_t **outputs, size_t *num_outputs) {
  if (!pipeline || !outputs || !num_outputs) return MODYN_ERROR_INVALID_ARGUMENT;
  pipeline_t *p = (pipeline_t*)pipeline;
  
  if (p->num_nodes == 0) {
    return modyn_run_inference((modyn_model_handle_t)0, inputs, num_inputs, outputs, num_outputs);
  }
  
  clock_t start_time = clock();
  
  size_t n = p->num_nodes;
  int *indegree = (int*)calloc(n, sizeof(int));
  if (!indegree) return MODYN_ERROR_MEMORY_ALLOCATION;
  
  for (size_t i = 0; i < p->num_edges; ++i) {
    if (p->edges[i].enabled) {
      indegree[p->edges[i].dst]++;
    }
  }
  
  int *queue = (int*)malloc(n * sizeof(int));
  if (!queue) { free(indegree); return MODYN_ERROR_MEMORY_ALLOCATION; }
  
  size_t qh = 0, qt = 0;
  for (size_t i = 0; i < n; ++i) {
    if (indegree[i] == 0) queue[qt++] = (int)i;
  }
  
  int last_sink = -1;
  modyn_pipeline_exec_context_t exec_context = {
    .pipeline = pipeline,
    .node_index = 0,
    .user_data = NULL,
    .iteration = 0,
    .max_iterations = 1000
  };
  
  while (qh < qt) {
    int u = queue[qh++];
    modyn_pipeline_node_t *node = p->nodes[u];
    
    modyn_tensor_data_t *in_tensors = inputs;
    size_t in_count = num_inputs;
    
    // 查找前驱节点的输出作为输入
    for (size_t e = 0; e < p->num_edges; ++e) {
      if (p->edges[e].dst == u && p->edges[e].enabled) {
        int pred = p->edges[e].src;
        if (p->nodes[pred]->cached_outputs && p->nodes[pred]->cached_num_outputs > 0) {
          in_tensors = p->nodes[pred]->cached_outputs;
          in_count = p->nodes[pred]->cached_num_outputs;
          break;
        }
      }
    }
    
    modyn_tensor_data_t *out_tensors = NULL;
    size_t out_count = 0;
    
    exec_context.node_index = u;
    modyn_status_e st = pipeline_execute_node(p, u, in_tensors, in_count, 
                                             &out_tensors, &out_count, &exec_context);
    if (st != MODYN_SUCCESS) { 
      free(indegree); free(queue); 
      p->failed_executions++;
      return st; 
    }
    
    node->cached_outputs = out_tensors;
    node->cached_num_outputs = out_count;
    
    // 拓扑排序推进
    int has_outgoing = 0;
    for (size_t e = 0; e < p->num_edges; ++e) {
      if (p->edges[e].src == u && p->edges[e].enabled) {
        has_outgoing = 1;
        int v = p->edges[e].dst;
        if (--indegree[v] == 0) queue[qt++] = v;
      }
    }
    if (!has_outgoing) last_sink = u;
  }
  
  free(indegree);
  free(queue);
  
  int result_node = (last_sink >= 0) ? last_sink : (int)(n - 1);
  modyn_pipeline_node_t *rn = p->nodes[result_node];
  *outputs = rn->cached_outputs;
  *num_outputs = rn->cached_num_outputs;
  rn->cached_outputs = NULL;
  rn->cached_num_outputs = 0;
  
  pipeline_free_cached_outputs(p, result_node);
  
  // 更新执行统计
  clock_t end_time = clock();
  p->total_execution_time_ms += (int)((end_time - start_time) * 1000 / CLOCKS_PER_SEC);
  p->total_executions++;
  p->successful_executions++;
  
  return MODYN_SUCCESS;
}

// ======================== Pipeline拓扑查询 ======================== //

// 查询pipeline拓扑
modyn_status_e modyn_query_pipeline_topology(modyn_pipeline_handle_t pipeline,
                                            modyn_pipeline_topology_t *topology) {
  if (!pipeline || !topology) return MODYN_ERROR_INVALID_ARGUMENT;
  pipeline_t *p = (pipeline_t*)pipeline;
  
  // 分配节点信息数组
  topology->nodes = (modyn_pipeline_node_info_t*)malloc(p->num_nodes * sizeof(modyn_pipeline_node_info_t));
  if (!topology->nodes) return MODYN_ERROR_MEMORY_ALLOCATION;
  
  // 分配边信息数组
  topology->edges = (modyn_pipeline_edge_info_t*)malloc(p->num_edges * sizeof(modyn_pipeline_edge_info_t));
  if (!topology->edges) {
    free(topology->nodes);
    return MODYN_ERROR_MEMORY_ALLOCATION;
  }
  
  // 填充基本信息
  topology->num_nodes = p->num_nodes;
  topology->num_edges = p->num_edges;
  strncpy(topology->name, p->name, sizeof(topology->name)-1);
  
  // 计算每个节点的入度和出度
  int *in_degree = (int*)calloc(p->num_nodes, sizeof(int));
  int *out_degree = (int*)calloc(p->num_nodes, sizeof(int));
  if (!in_degree || !out_degree) {
    free(topology->nodes);
    free(topology->edges);
    free(in_degree);
    free(out_degree);
    return MODYN_ERROR_MEMORY_ALLOCATION;
  }
  
  // 统计边信息
  for (size_t i = 0; i < p->num_edges; ++i) {
    if (p->edges[i].enabled) {
      in_degree[p->edges[i].dst]++;
      out_degree[p->edges[i].src]++;
      
      // 填充边信息
      strncpy(topology->edges[i].src_node, p->nodes[p->edges[i].src]->name, 
               sizeof(topology->edges[i].src_node)-1);
      strncpy(topology->edges[i].dst_node, p->nodes[p->edges[i].dst]->name, 
               sizeof(topology->edges[i].dst_node)-1);
      topology->edges[i].src_output_idx = p->edges[i].src_out;
      topology->edges[i].dst_input_idx = p->edges[i].dst_in;
    }
  }
  
  // 填充节点信息
  for (size_t i = 0; i < p->num_nodes; ++i) {
    modyn_pipeline_node_t *node = p->nodes[i];
    strncpy(topology->nodes[i].name, node->name, sizeof(topology->nodes[i].name)-1);
    
    // 根据节点类型设置模型句柄
    if (node->type == MODYN_PIPELINE_NODE_MODEL) {
      // 对于模型节点，从私有数据中获取模型句柄
      topology->nodes[i].model_handle = node->private_data;
    } else {
      topology->nodes[i].model_handle = NULL;
    }
    
    topology->nodes[i].is_source = (in_degree[i] == 0);
    topology->nodes[i].is_sink = (out_degree[i] == 0);
    
    // 获取输入输出数量（这里简化处理）
    topology->nodes[i].num_inputs = 1;
    topology->nodes[i].num_outputs = 1;
  }
  
  free(in_degree);
  free(out_degree);
  return MODYN_SUCCESS;
}

// 释放pipeline拓扑信息
modyn_status_e modyn_free_pipeline_topology(modyn_pipeline_topology_t *topology) {
  if (!topology) return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (topology->nodes) {
    free(topology->nodes);
    topology->nodes = NULL;
  }
  
  if (topology->edges) {
    free(topology->edges);
    topology->edges = NULL;
  }
  
  topology->num_nodes = 0;
  topology->num_edges = 0;
  memset(topology->name, 0, sizeof(topology->name));
  
  return MODYN_SUCCESS;
}

// ======================== 节点类型管理 ======================== //

// 注册节点类型工厂
modyn_status_e modyn_register_pipeline_node_type(modyn_pipeline_node_type_e node_type,
                                                const char *type_name,
                                                modyn_pipeline_node_create_fn create_func) {
  if (!type_name || !create_func) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 检查是否已存在
  modyn_pipeline_node_factory_t *existing = g_node_factories;
  while (existing) {
    if (existing->node_type == node_type) {
      return MODYN_ERROR_INVALID_ARGUMENT; // 已存在
    }
    existing = existing->next;
  }
  
  // 创建新的工厂
  modyn_pipeline_node_factory_t *new_factory = (modyn_pipeline_node_factory_t*)malloc(sizeof(modyn_pipeline_node_factory_t));
  if (!new_factory) return MODYN_ERROR_MEMORY_ALLOCATION;
  
  new_factory->node_type = node_type;
  new_factory->type_name = type_name;  // 直接赋值字符串指针
  new_factory->create_func = create_func;
  new_factory->next = g_node_factories;
  g_node_factories = new_factory;
  
  return MODYN_SUCCESS;
}

// 注销节点类型工厂
modyn_status_e modyn_unregister_pipeline_node_type(modyn_pipeline_node_type_e node_type) {
  modyn_pipeline_node_factory_t *prev = NULL;
  modyn_pipeline_node_factory_t *current = g_node_factories;
  
  while (current) {
    if (current->node_type == node_type) {
      if (prev) {
        prev->next = current->next;
      } else {
        g_node_factories = current->next;
      }
      free(current);
      return MODYN_SUCCESS;
    }
    prev = current;
    current = current->next;
  }
  
  return MODYN_ERROR_INVALID_ARGUMENT; // 未找到
}

// ======================== Pipeline控制与监控 ======================== //

// 设置pipeline执行选项
modyn_status_e modyn_pipeline_set_execution_options(modyn_pipeline_handle_t pipeline,
                                                   int timeout_ms,
                                                   int max_retries,
                                                   int enable_parallel) {
  if (!pipeline) return MODYN_ERROR_INVALID_ARGUMENT;
  pipeline_t *p = (pipeline_t*)pipeline;
  
  p->timeout_ms = timeout_ms > 0 ? timeout_ms : 30000;
  p->max_retries = max_retries > 0 ? max_retries : 3;
  p->enable_parallel = enable_parallel ? 1 : 0;
  
  return MODYN_SUCCESS;
}

// 获取pipeline执行统计
modyn_status_e modyn_pipeline_get_execution_stats(modyn_pipeline_handle_t pipeline,
                                                 size_t *total_nodes,
                                                 size_t *executed_nodes,
                                                 size_t *skipped_nodes,
                                                 size_t *error_nodes,
                                                 int *total_time_ms) {
  if (!pipeline) return MODYN_ERROR_INVALID_ARGUMENT;
  pipeline_t *p = (pipeline_t*)pipeline;
  
  if (total_nodes) *total_nodes = p->num_nodes;
  if (executed_nodes) *executed_nodes = p->successful_executions;
  if (skipped_nodes) *skipped_nodes = 0; // 当前实现中没有跳过逻辑
  if (error_nodes) *error_nodes = p->failed_executions;
  if (total_time_ms) *total_time_ms = p->total_execution_time_ms;
  
  return MODYN_SUCCESS;
}

// ======================== 节点管理内部函数 ======================== //

// 全局注册节点
static modyn_status_e register_node_globally(modyn_pipeline_node_t *node) {
  if (!node || !node->name[0]) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 检查是否已存在
  if (find_node_globally(node->name)) {
    return MODYN_ERROR_INVALID_ARGUMENT; // 节点名已存在
  }
  
  // 扩展数组
  if (g_num_registered_nodes == g_cap_registered_nodes) {
    size_t new_cap = g_cap_registered_nodes ? g_cap_registered_nodes * 2 : 8;
    modyn_pipeline_node_t **new_nodes = (modyn_pipeline_node_t**)realloc(
      g_registered_nodes, new_cap * sizeof(modyn_pipeline_node_t*));
    if (!new_nodes) return MODYN_ERROR_MEMORY_ALLOCATION;
    g_registered_nodes = new_nodes;
    g_cap_registered_nodes = new_cap;
  }
  
  g_registered_nodes[g_num_registered_nodes++] = node;
  return MODYN_SUCCESS;
}

// 全局注销节点
static modyn_status_e unregister_node_globally(const char *node_name) {
  if (!node_name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  for (size_t i = 0; i < g_num_registered_nodes; ++i) {
    if (strcmp(g_registered_nodes[i]->name, node_name) == 0) {
      // 移动后面的节点
      for (size_t j = i; j < g_num_registered_nodes - 1; ++j) {
        g_registered_nodes[j] = g_registered_nodes[j + 1];
      }
      g_num_registered_nodes--;
      return MODYN_SUCCESS;
    }
  }
  
  return MODYN_ERROR_INVALID_ARGUMENT; // 未找到节点
}

// 全局查找节点
static modyn_pipeline_node_t* find_node_globally(const char *node_name) {
  if (!node_name) return NULL;
  
  for (size_t i = 0; i < g_num_registered_nodes; ++i) {
    if (strcmp(g_registered_nodes[i]->name, node_name) == 0) {
      return g_registered_nodes[i];
    }
  }
  
  return NULL;
}

// 检查节点是否匹配搜索条件
static int match_node_criteria(const modyn_pipeline_node_t *node, 
                              const modyn_node_search_criteria_t *criteria) {
  if (!node || !criteria) return 0;
  
  // 检查节点类型
  if (criteria->node_type >= 0 && node->type != criteria->node_type) {
    return 0;
  }
  
  // 检查名称模式
  if (criteria->name_pattern && !wildcard_match(criteria->name_pattern, node->name)) {
    return 0;
  }
  
  // 检查标签（暂时简化，实际应该支持多个标签）
  if (criteria->tag && strcmp(criteria->tag, node->config.name) != 0) {
    return 0;
  }
  
  // 检查优先级范围
  if (criteria->min_priority > 0 && node->config.priority < criteria->min_priority) {
    return 0;
  }
  if (criteria->max_priority > 0 && node->config.priority > criteria->max_priority) {
    return 0;
  }
  
  return 1;
}

// 通配符匹配（简化实现）
static int wildcard_match(const char *pattern, const char *text) {
  if (!pattern || !text) return 0;
  
  // 简单的通配符匹配：* 匹配任意字符串
  if (strcmp(pattern, "*") == 0) return 1;
  if (strcmp(pattern, text) == 0) return 1;
  
  // 检查是否以模式开头
  size_t pattern_len = strlen(pattern);
  if (pattern_len > 0 && pattern[pattern_len-1] == '*') {
    return strncmp(pattern, text, pattern_len-1) == 0;
  }
  
  return 0;
}

// ======================== 公共节点管理API ======================== //

// 查找匹配条件的节点
modyn_status_e modyn_find_nodes(const modyn_node_search_criteria_t *criteria,
                                modyn_node_info_t *nodes,
                                size_t max_nodes,
                                size_t *num_found) {
  if (!criteria || !nodes || !num_found) return MODYN_ERROR_INVALID_ARGUMENT;
  
  *num_found = 0;
  size_t found = 0;
  
  for (size_t i = 0; i < g_num_registered_nodes && found < max_nodes; ++i) {
    modyn_pipeline_node_t *node = g_registered_nodes[i];
    if (match_node_criteria(node, criteria)) {
      // 填充节点信息
      strncpy(nodes[found].name, node->name, sizeof(nodes[found].name)-1);
      nodes[found].type = node->type;
      nodes[found].type_name = "unknown"; // 暂时硬编码，实际应该从工厂获取
      strncpy(nodes[found].tag, "", sizeof(nodes[found].tag)-1); // 暂时为空
      nodes[found].priority = node->config.priority;
      nodes[found].enabled = node->config.enabled;
      nodes[found].execution_count = node->execution_count;
      nodes[found].success_count = node->success_count;
      nodes[found].error_count = node->error_count;
      nodes[found].total_time_ms = node->total_time_ms;
      nodes[found].private_data = node->private_data;
      
      found++;
    }
  }
  
  *num_found = found;
  return MODYN_SUCCESS;
}

// 根据名称获取节点
modyn_status_e modyn_get_node_by_name(const char *node_name,
                                     modyn_node_info_t *node_info) {
  if (!node_name || !node_info) return MODYN_ERROR_INVALID_ARGUMENT;
  
  modyn_pipeline_node_t *node = find_node_globally(node_name);
  if (!node) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 填充节点信息
  strncpy(node_info->name, node->name, sizeof(node_info->name)-1);
  node_info->type = node->type;
  node_info->type_name = "unknown"; // 暂时硬编码
  strncpy(node_info->tag, "", sizeof(node_info->tag)-1);
  node_info->priority = node->config.priority;
  node_info->enabled = node->config.enabled;
  node_info->execution_count = node->execution_count;
  node_info->success_count = node->success_count;
  node_info->error_count = node->error_count;
  node_info->total_time_ms = node->total_time_ms;
  node_info->private_data = node->private_data;
  
  return MODYN_SUCCESS;
}

// 获取指定类型的所有节点
modyn_status_e modyn_get_nodes_by_type(modyn_pipeline_node_type_e node_type,
                                      modyn_node_info_t *nodes,
                                      size_t max_nodes,
                                      size_t *num_found) {
  if (!nodes || !num_found) return MODYN_ERROR_INVALID_ARGUMENT;
  
  modyn_node_search_criteria_t criteria = {
    .node_type = node_type,
    .name_pattern = NULL,
    .tag = NULL,
    .min_priority = 0,
    .max_priority = 0,
    .enabled_only = 0
  };
  
  return modyn_find_nodes(&criteria, nodes, max_nodes, num_found);
}

// 为节点添加标签
modyn_status_e modyn_tag_node(const char *node_name, const char *tag) {
  if (!node_name || !tag) return MODYN_ERROR_INVALID_ARGUMENT;
  
  modyn_pipeline_node_t *node = find_node_globally(node_name);
  if (!node) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 暂时简化实现，实际应该支持多个标签
  // 这里我们暂时不实现标签功能，只是返回成功
  return MODYN_SUCCESS;
}

// 移除节点标签
modyn_status_e modyn_untag_node(const char *node_name, const char *tag) {
  if (!node_name || !tag) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 暂时简化实现
  return MODYN_SUCCESS;
}

// 获取节点统计信息
modyn_status_e modyn_get_node_stats(const char *node_name,
                                   modyn_node_info_t *stats) {
  return modyn_get_node_by_name(node_name, stats);
}
