#include#include "core/unified_pipeline.h"
#include "core/inference_engine.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

// ================================
// Tensor Map 实现
// ================================

tensor_map_t* tensor_map_create(uint32_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 16;
    }
    
    tensor_map_t* map = calloc(1, sizeof(tensor_map_t));
    if (!map) {
        return NULL;
    }
    
    map->keys = calloc(initial_capacity, sizeof(char*));
    map->tensors = calloc(initial_capacity, sizeof(tensor_t*));
    
    if (!map->keys || !map->tensors) {
        free(map->keys);
        free(map->tensors);
        free(map);
        return NULL;
    }
    
    map->count = 0;
    map->capacity = initial_capacity;
    
    return map;
}

void tensor_map_destroy(tensor_map_t* map) {
    if (!map) return;
    
    for (uint32_t i = 0; i < map->count; i++) {
        free(map->keys[i]);
        // 注意：这里不释放tensor本身，因为它们可能被其他地方引用
    }
    
    free(map->keys);
    free(map->tensors);
    free(map);
}

static int tensor_map_resize(tensor_map_t* map) {
    uint32_t new_capacity = map->capacity * 2;
    
    char** new_keys = realloc(map->keys, new_capacity * sizeof(char*));
    tensor_t** new_tensors = realloc(map->tensors, new_capacity * sizeof(tensor_t*));
    
    if (!new_keys || !new_tensors) {
        return -1;
    }
    
    map->keys = new_keys;
    map->tensors = new_tensors;
    map->capacity = new_capacity;
    
    // 初始化新分配的空间
    for (uint32_t i = map->count; i < new_capacity; i++) {
        map->keys[i] = NULL;
        map->tensors[i] = NULL;
    }
    
    return 0;
}

int tensor_map_set(tensor_map_t* map, const char* key, tensor_t* tensor) {
    if (!map || !key || !tensor) {
        return -1;
    }
    
    // 检查是否已存在
    for (uint32_t i = 0; i < map->count; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            // 更新现有条目
            map->tensors[i] = tensor;
            return 0;
        }
    }
    
    // 检查是否需要扩容
    if (map->count >= map->capacity) {
        if (tensor_map_resize(map) != 0) {
            return -1;
        }
    }
    
    // 添加新条目
    map->keys[map->count] = strdup(key);
    map->tensors[map->count] = tensor;
    map->count++;
    
    return 0;
}

tensor_t* tensor_map_get(const tensor_map_t* map, const char* key) {
    if (!map || !key) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < map->count; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            return map->tensors[i];
        }
    }
    
    return NULL;
}

bool tensor_map_has(const tensor_map_t* map, const char* key) {
    return tensor_map_get(map, key) != NULL;
}

int tensor_map_remove(tensor_map_t* map, const char* key) {
    if (!map || !key) {
        return -1;
    }
    
    for (uint32_t i = 0; i < map->count; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            // 释放key内存
            free(map->keys[i]);
            
            // 将后面的元素前移
            for (uint32_t j = i; j < map->count - 1; j++) {
                map->keys[j] = map->keys[j + 1];
                map->tensors[j] = map->tensors[j + 1];
            }
            
            map->count--;
            map->keys[map->count] = NULL;
            map->tensors[map->count] = NULL;
            
            return 0;
        }
    }
    
    return -1; // 未找到
}

uint32_t tensor_map_size(const tensor_map_t* map) {
    return map ? map->count : 0;
}

void tensor_map_clear(tensor_map_t* map) {
    if (!map) return;
    
    for (uint32_t i = 0; i < map->count; i++) {
        free(map->keys[i]);
        map->keys[i] = NULL;
        map->tensors[i] = NULL;
    }
    
    map->count = 0;
}

tensor_map_t* tensor_map_copy(const tensor_map_t* src) {
    if (!src) {
        return NULL;
    }
    
    tensor_map_t* dst = tensor_map_create(src->capacity);
    if (!dst) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < src->count; i++) {
        if (tensor_map_set(dst, src->keys[i], src->tensors[i]) != 0) {
            tensor_map_destroy(dst);
            return NULL;
        }
    }
    
    return dst;
}

// ================================
// Processing Unit 基础实现
// ================================

static processing_unit_t* processing_unit_create_base(const char* name, unit_type_e type) {
    processing_unit_t* unit = calloc(1, sizeof(processing_unit_t));
    if (!unit) {
        return NULL;
    }
    
    strncpy(unit->name, name, sizeof(unit->name) - 1);
    unit->type = type;
    unit->timeout_ms = 30000; // 默认30秒超时
    
    return unit;
}

processing_unit_t* create_function_unit(const char* name, 
                                       process_func_t process_func,
                                       void* context,
                                       const char** input_keys, uint32_t input_count,
                                       const char** output_keys, uint32_t output_count) {
    if (!name || !process_func) {
        return NULL;
    }
    
    processing_unit_t* unit = processing_unit_create_base(name, UNIT_TYPE_FUNCTION);
    if (!unit) {
        return NULL;
    }
    
    unit->process = process_func;
    unit->context = context;
    unit->input_count = input_count;
    unit->output_count = output_count;
    
    // 复制输入键名
    if (input_count > 0 && input_keys) {
        unit->input_keys = malloc(input_count * sizeof(char*));
        if (!unit->input_keys) {
            free(unit);
            return NULL;
        }
        
        for (uint32_t i = 0; i < input_count; i++) {
            unit->input_keys[i] = strdup(input_keys[i]);
        }
    }
    
    // 复制输出键名
    if (output_count > 0 && output_keys) {
        unit->output_keys = malloc(output_count * sizeof(char*));
        if (!unit->output_keys) {
            for (uint32_t i = 0; i < input_count; i++) {
                free(unit->input_keys[i]);
            }
            free(unit->input_keys);
            free(unit);
            return NULL;
        }
        
        for (uint32_t i = 0; i < output_count; i++) {
            unit->output_keys[i] = strdup(output_keys[i]);
        }
    }
    
    return unit;
}

// ================================
// 模型推理单元实现
// ================================

// 模型配置结构
typedef struct {
    char* model_path;
    infer_engine_t engine;
    bool engine_loaded;
} model_unit_config_t;

// 模型推理处理函数
static int model_process_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    model_unit_config_t* config = (model_unit_config_t*)context;
    if (!config || !config->engine_loaded) {
        return -1;
    }
    
    // 暂时简化实现：假设单输入单输出
    if (inputs->count == 0) {
        return -1;
    }
    
    tensor_t* input_tensor = inputs->tensors[0];
    if (!input_tensor) {
        return -1;
    }
    
    // 创建输出tensor（这里需要根据模型规格来确定）
    uint32_t output_dims[] = {1, 1000}; // 假设是1000类分类
    tensor_shape_t output_shape = tensor_shape_create(output_dims, 2);
    tensor_t* output_tensor = malloc(sizeof(tensor_t));
    *output_tensor = tensor_create("model_output", TENSOR_TYPE_FLOAT32, &output_shape, TENSOR_FORMAT_NC);
    
    output_tensor->data = malloc(output_tensor->size);
    output_tensor->owns_data = true;
    
    if (!output_tensor->data) {
        tensor_free(output_tensor);
        free(output_tensor);
        return -1;
    }
    
    // 执行推理（这里应该调用实际的推理引擎）
    // 暂时填充虚拟数据
    float* output_data = (float*)output_tensor->data;
    for (uint32_t i = 0; i < 1000; i++) {
        output_data[i] = (float)(rand() % 1000) / 1000.0f;
    }
    
    // 添加到输出映射表
    if (outputs->count > 0 && outputs->keys[0]) {
        tensor_map_set((tensor_map_t*)outputs, outputs->keys[0], output_tensor);
    } else {
        tensor_map_set((tensor_map_t*)outputs, "model_output", output_tensor);
    }
    
    return 0;
}

processing_unit_t* create_model_unit(const char* name,
                                    const char* model_path,
                                    const char** input_keys, uint32_t input_count,
                                    const char** output_keys, uint32_t output_count) {
    if (!name || !model_path) {
        return NULL;
    }
    
    // 创建模型配置
    model_unit_config_t* config = calloc(1, sizeof(model_unit_config_t));
    if (!config) {
        return NULL;
    }
    
    config->model_path = strdup(model_path);
    config->engine_loaded = false;
    
    // 这里应该加载模型，暂时简化
    // config->engine = infer_engine_create(...);
    // config->engine_loaded = true;
    
    processing_unit_t* unit = create_function_unit(name, model_process_func, config,
                                                  input_keys, input_count,
                                                  output_keys, output_count);
    
    if (!unit) {
        free(config->model_path);
        free(config);
        return NULL;
    }
    
    unit->type = UNIT_TYPE_MODEL;
    unit->config = config;
    
    return unit;
}

// ================================
// 其他处理单元暂时简化实现
// ================================

processing_unit_t* create_parallel_unit(const char* name,
                                       processing_unit_t** sub_units,
                                       uint32_t unit_count) {
    // 暂时返回NULL，实际需要实现并行逻辑
    (void)name; (void)sub_units; (void)unit_count;
    return NULL;
}

processing_unit_t* create_conditional_unit(const char* name,
                                          process_func_t condition_func,
                                          processing_unit_t* true_unit,
                                          processing_unit_t* false_unit) {
    // 暂时返回NULL，实际需要实现条件逻辑
    (void)name; (void)condition_func; (void)true_unit; (void)false_unit;
    return NULL;
}

processing_unit_t* create_loop_unit(const char* name,
                                   process_func_t loop_condition,
                                   processing_unit_t* loop_body,
                                   uint32_t max_iterations) {
    // 暂时返回NULL，实际需要实现循环逻辑
    (void)name; (void)loop_condition; (void)loop_body; (void)max_iterations;
    return NULL;
}

void processing_unit_destroy(processing_unit_t* unit) {
    if (!unit) return;
    
    // 释放输入键名
    for (uint32_t i = 0; i < unit->input_count; i++) {
        free(unit->input_keys[i]);
    }
    free(unit->input_keys);
    
    // 释放输出键名
    for (uint32_t i = 0; i < unit->output_count; i++) {
        free(unit->output_keys[i]);
    }
    free(unit->output_keys);
    
    // 释放类型特定的配置
    switch (unit->type) {
        case UNIT_TYPE_MODEL: {
            model_unit_config_t* config = (model_unit_config_t*)unit->config;
            if (config) {
                free(config->model_path);
                // 这里应该卸载模型
                free(config);
            }
            break;
        }
        default:
            break;
    }
    
    free(unit);
}

int processing_unit_execute(processing_unit_t* unit, 
                           const tensor_map_t* inputs, 
                           tensor_map_t* outputs) {
    if (!unit || !inputs || !outputs) {
        return -1;
    }
    
    // 检查输入是否满足要求
    for (uint32_t i = 0; i < unit->input_count; i++) {
        if (!tensor_map_has(inputs, unit->input_keys[i])) {
            printf("错误：缺少输入张量 '%s'\n", unit->input_keys[i]);
            return -1;
        }
    }
    
    // 执行处理函数
    if (unit->process) {
        return unit->process(inputs, outputs, unit->context);
    }
    
    return -1;
}

// ================================
// Unified Pipeline 实现
// ================================

unified_pipeline_t* unified_pipeline_create(const char* name) {
    if (!name) {
        return NULL;
    }
    
    unified_pipeline_t* pipeline = calloc(1, sizeof(unified_pipeline_t));
    if (!pipeline) {
        return NULL;
    }
    
    strncpy(pipeline->name, name, sizeof(pipeline->name) - 1);
    pipeline->capacity = 16;
    pipeline->units = calloc(pipeline->capacity, sizeof(processing_unit_t*));
    
    if (!pipeline->units) {
        free(pipeline);
        return NULL;
    }
    
    pipeline->global_map = tensor_map_create(32);
    if (!pipeline->global_map) {
        free(pipeline->units);
        free(pipeline);
        return NULL;
    }
    
    return pipeline;
}

void unified_pipeline_destroy(unified_pipeline_t* pipeline) {
    if (!pipeline) return;
    
    // 销毁所有处理单元
    for (uint32_t i = 0; i < pipeline->unit_count; i++) {
        processing_unit_destroy(pipeline->units[i]);
    }
    
    free(pipeline->units);
    tensor_map_destroy(pipeline->global_map);
    free(pipeline);
}

int unified_pipeline_add_unit(unified_pipeline_t* pipeline, processing_unit_t* unit) {
    if (!pipeline || !unit) {
        return -1;
    }
    
    // 检查是否需要扩容
    if (pipeline->unit_count >= pipeline->capacity) {
        uint32_t new_capacity = pipeline->capacity * 2;
        processing_unit_t** new_units = realloc(pipeline->units, 
                                                new_capacity * sizeof(processing_unit_t*));
        if (!new_units) {
            return -1;
        }
        
        pipeline->units = new_units;
        pipeline->capacity = new_capacity;
        
        // 初始化新分配的空间
        for (uint32_t i = pipeline->unit_count; i < new_capacity; i++) {
            pipeline->units[i] = NULL;
        }
    }
    
    pipeline->units[pipeline->unit_count] = unit;
    pipeline->unit_count++;
    
    return 0;
}

// 获取当前时间（毫秒）
static double get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int unified_pipeline_execute(unified_pipeline_t* pipeline,
                            const tensor_map_t* inputs,
                            tensor_map_t* outputs) {
    if (!pipeline || !inputs || !outputs) {
        return -1;
    }
    
    // 将输入复制到全局映射表
    tensor_map_clear(pipeline->global_map);
    for (uint32_t i = 0; i < inputs->count; i++) {
        tensor_map_set(pipeline->global_map, inputs->keys[i], inputs->tensors[i]);
    }
    
    // 依次执行每个处理单元
    for (uint32_t i = 0; i < pipeline->unit_count; i++) {
        processing_unit_t* unit = pipeline->units[i];
        
        if (pipeline->debug_mode) {
            printf("执行处理单元: %s\n", unit->name);
        }
        
        double start_time = get_current_time_ms();
        
        // 执行处理单元
        int result = processing_unit_execute(unit, pipeline->global_map, pipeline->global_map);
        
        double end_time = get_current_time_ms();
        double execution_time = end_time - start_time;
        
        if (pipeline->debug_mode) {
            printf("处理单元 '%s' 执行完成: 结果=%d, 耗时=%.2fms\n", 
                   unit->name, result, execution_time);
        }
        
        if (result != 0) {
            printf("错误：处理单元 '%s' 执行失败，错误代码: %d\n", unit->name, result);
            return result;
        }
    }
    
    // 将最终结果复制到输出
    for (uint32_t i = 0; i < pipeline->global_map->count; i++) {
        tensor_map_set((tensor_map_t*)outputs, 
                      pipeline->global_map->keys[i], 
                      pipeline->global_map->tensors[i]);
    }
    
    return 0;
}

// 暂时简化其他功能的实现
int unified_pipeline_execute_async(unified_pipeline_t* pipeline,
                                  const tensor_map_t* inputs,
                                  tensor_map_t* outputs,
                                  pipeline_completion_callback_t callback,
                                  void* user_data) {
    // 暂时同步执行
    int result = unified_pipeline_execute(pipeline, inputs, outputs);
    if (callback) {
        callback(pipeline, result, user_data);
    }
    return result;
}

int unified_pipeline_set_memory_pool(unified_pipeline_t* pipeline, void* memory_pool) {
    if (!pipeline) {
        return -1;
    }
    
    pipeline->memory_pool = memory_pool;
    pipeline->enable_memory_pool = (memory_pool != NULL);
    
    return 0;
}

void unified_pipeline_set_debug_mode(unified_pipeline_t* pipeline, bool enable) {
    if (pipeline) {
        pipeline->debug_mode = enable;
    }
}

void unified_pipeline_set_unit_callback(unified_pipeline_t* pipeline, unit_execution_callback_t callback) {
    // 暂时简化实现
    (void)pipeline;
    (void)callback;
}

int unified_pipeline_get_stats(unified_pipeline_t* pipeline,
                              uint32_t* total_units,
                              double* total_execution_time,
                              double* avg_unit_time) {
    if (!pipeline) {
        return -1;
    }
    
    if (total_units) {
        *total_units = pipeline->unit_count;
    }
    
    // 暂时返回虚拟数据，实际应该记录执行统计
    if (total_execution_time) {
        *total_execution_time = 0.0;
    }
    
    if (avg_unit_time) {
        *avg_unit_time = 0.0;
    }
    
    return 0;
}

// ================================
// 常用处理函数实现
// ================================

int image_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* image = tensor_map_get(inputs, "image");
    if (!image) {
        return -1;
    }
    
    // 创建处理后的图像tensor（简化实现）
    tensor_t* processed_image = malloc(sizeof(tensor_t));
    *processed_image = tensor_copy(image);
    
    // 这里应该进行实际的图像预处理：调整大小、归一化等
    
    tensor_map_set((tensor_map_t*)outputs, "processed_image", processed_image);
    
    return 0;
}

int text_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* text = tensor_map_get(inputs, "text");
    if (!text) {
        return -1;
    }
    
    // 创建token tensor（简化实现）
    uint32_t token_dims[] = {1, 512}; // 假设最大512个token
    tensor_shape_t token_shape = tensor_shape_create(token_dims, 2);
    tensor_t* tokens = malloc(sizeof(tensor_t));
    *tokens = tensor_create("tokens", TENSOR_TYPE_INT32, &token_shape, TENSOR_FORMAT_NC);
    
    tokens->data = malloc(tokens->size);
    tokens->owns_data = true;
    
    // 这里应该进行实际的文本预处理：分词、编码等
    // 暂时填充虚拟token
    int32_t* token_data = (int32_t*)tokens->data;
    for (uint32_t i = 0; i < 512; i++) {
        token_data[i] = rand() % 30000; // 假设词汇表大小为30000
    }
    
    tensor_map_set((tensor_map_t*)outputs, "tokens", tokens);
    
    return 0;
}

int audio_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* audio = tensor_map_get(inputs, "audio");
    if (!audio) {
        return -1;
    }
    
    // 创建特征tensor（简化实现）
    uint32_t feature_dims[] = {1, 80, 100}; // 假设80个mel频段，100帧
    tensor_shape_t feature_shape = tensor_shape_create(feature_dims, 3);
    tensor_t* features = malloc(sizeof(tensor_t));
    *features = tensor_create("features", TENSOR_TYPE_FLOAT32, &feature_shape, TENSOR_FORMAT_NCHW);
    
    features->data = malloc(features->size);
    features->owns_data = true;
    
    // 这里应该进行实际的音频预处理：重采样、MFCC/Mel谱等
    // 暂时填充虚拟特征
    float* feature_data = (float*)features->data;
    for (uint32_t i = 0; i < 80 * 100; i++) {
        feature_data[i] = (float)(rand() % 1000) / 1000.0f;
    }
    
    tensor_map_set((tensor_map_t*)outputs, "features", features);
    
    return 0;
}

int classification_postprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* logits = tensor_map_get(inputs, "logits");
    if (!logits) {
        return -1;
    }
    
    // 应用softmax并找到最大值（简化实现）
    float* logit_data = (float*)logits->data;
    uint32_t num_classes = logits->shape.dims[1];
    
    // 创建预测结果tensor
    uint32_t pred_dims[] = {1};
    tensor_shape_t pred_shape = tensor_shape_create(pred_dims, 1);
    tensor_t* predictions = malloc(sizeof(tensor_t));
    *predictions = tensor_create("predictions", TENSOR_TYPE_INT32, &pred_shape, TENSOR_FORMAT_N);
    
    predictions->data = malloc(predictions->size);
    predictions->owns_data = true;
    
    // 创建概率tensor
    tensor_t* probabilities = malloc(sizeof(tensor_t));
    *probabilities = tensor_copy(logits);
    probabilities->name = strdup("probabilities");
    
    // 简化的softmax和argmax
    float max_logit = logit_data[0];
    int32_t max_index = 0;
    
    for (uint32_t i = 1; i < num_classes; i++) {
        if (logit_data[i] > max_logit) {
            max_logit = logit_data[i];
            max_index = i;
        }
    }
    
    *((int32_t*)predictions->data) = max_index;
    
    tensor_map_set((tensor_map_t*)outputs, "predictions", predictions);
    tensor_map_set((tensor_map_t*)outputs, "probabilities", probabilities);
    
    return 0;
}

int audio_postprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* raw_audio = tensor_map_get(inputs, "raw_audio");
    if (!raw_audio) {
        return -1;
    }
    
    // 创建增强后的音频tensor（简化实现）
    tensor_t* enhanced_audio = malloc(sizeof(tensor_t));
    *enhanced_audio = tensor_copy(raw_audio);
    enhanced_audio->name = strdup("enhanced_audio");
    
    // 这里应该进行实际的音频后处理：降噪、增强等
    // 暂时直接复制数据
    
    tensor_map_set((tensor_map_t*)outputs, "enhanced_audio", enhanced_audio);
    
    return 0;
}"core/unified_pipeline.h"
#include "core/inference_engine.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

// ================================
// Tensor Map 实现
// ================================

tensor_map_t* tensor_map_create(uint32_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 16;
    }
    
    tensor_map_t* map = calloc(1, sizeof(tensor_map_t));
    if (!map) {
        return NULL;
    }
    
    map->keys = calloc(initial_capacity, sizeof(char*));
    map->tensors = calloc(initial_capacity, sizeof(tensor_t*));
    
    if (!map->keys || !map->tensors) {
        free(map->keys);
        free(map->tensors);
        free(map);
        return NULL;
    }
    
    map->count = 0;
    map->capacity = initial_capacity;
    
    return map;
}

void tensor_map_destroy(tensor_map_t* map) {
    if (!map) return;
    
    for (uint32_t i = 0; i < map->count; i++) {
        free(map->keys[i]);
        // 注意：这里不释放tensor本身，因为它们可能被其他地方引用
    }
    
    free(map->keys);
    free(map->tensors);
    free(map);
}

static int tensor_map_resize(tensor_map_t* map) {
    uint32_t new_capacity = map->capacity * 2;
    
    char** new_keys = realloc(map->keys, new_capacity * sizeof(char*));
    tensor_t** new_tensors = realloc(map->tensors, new_capacity * sizeof(tensor_t*));
    
    if (!new_keys || !new_tensors) {
        return -1;
    }
    
    map->keys = new_keys;
    map->tensors = new_tensors;
    map->capacity = new_capacity;
    
    // 初始化新分配的空间
    for (uint32_t i = map->count; i < new_capacity; i++) {
        map->keys[i] = NULL;
        map->tensors[i] = NULL;
    }
    
    return 0;
}

int tensor_map_set(tensor_map_t* map, const char* key, tensor_t* tensor) {
    if (!map || !key || !tensor) {
        return -1;
    }
    
    // 检查是否已存在
    for (uint32_t i = 0; i < map->count; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            // 更新现有条目
            map->tensors[i] = tensor;
            return 0;
        }
    }
    
    // 检查是否需要扩容
    if (map->count >= map->capacity) {
        if (tensor_map_resize(map) != 0) {
            return -1;
        }
    }
    
    // 添加新条目
    map->keys[map->count] = strdup(key);
    map->tensors[map->count] = tensor;
    map->count++;
    
    return 0;
}

tensor_t* tensor_map_get(const tensor_map_t* map, const char* key) {
    if (!map || !key) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < map->count; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            return map->tensors[i];
        }
    }
    
    return NULL;
}

bool tensor_map_has(const tensor_map_t* map, const char* key) {
    return tensor_map_get(map, key) != NULL;
}

int tensor_map_remove(tensor_map_t* map, const char* key) {
    if (!map || !key) {
        return -1;
    }
    
    for (uint32_t i = 0; i < map->count; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            // 释放key内存
            free(map->keys[i]);
            
            // 将后面的元素前移
            for (uint32_t j = i; j < map->count - 1; j++) {
                map->keys[j] = map->keys[j + 1];
                map->tensors[j] = map->tensors[j + 1];
            }
            
            map->count--;
            map->keys[map->count] = NULL;
            map->tensors[map->count] = NULL;
            
            return 0;
        }
    }
    
    return -1; // 未找到
}

uint32_t tensor_map_size(const tensor_map_t* map) {
    return map ? map->count : 0;
}

void tensor_map_clear(tensor_map_t* map) {
    if (!map) return;
    
    for (uint32_t i = 0; i < map->count; i++) {
        free(map->keys[i]);
        map->keys[i] = NULL;
        map->tensors[i] = NULL;
    }
    
    map->count = 0;
}

tensor_map_t* tensor_map_copy(const tensor_map_t* src) {
    if (!src) {
        return NULL;
    }
    
    tensor_map_t* dst = tensor_map_create(src->capacity);
    if (!dst) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < src->count; i++) {
        if (tensor_map_set(dst, src->keys[i], src->tensors[i]) != 0) {
            tensor_map_destroy(dst);
            return NULL;
        }
    }
    
    return dst;
}

// ================================
// Processing Unit 实现
// ================================

static processing_unit_t* processing_unit_create_base(const char* name, unit_type_e type) {
    processing_unit_t* unit = calloc(1, sizeof(processing_unit_t));
    if (!unit) {
        return NULL;
    }
    
    strncpy(unit->name, name, sizeof(unit->name) - 1);
    unit->type = type;
    unit->timeout_ms = 30000; // 默认30秒超时
    
    return unit;
}

processing_unit_t* create_function_unit(const char* name, 
                                       process_func_t process_func,
                                       void* context,
                                       const char** input_keys, uint32_t input_count,
                                       const char** output_keys, uint32_t output_count) {
    if (!name || !process_func) {
        return NULL;
    }
    
    processing_unit_t* unit = processing_unit_create_base(name, UNIT_TYPE_FUNCTION);
    if (!unit) {
        return NULL;
    }
    
    unit->process = process_func;
    unit->context = context;
    unit->input_count = input_count;
    unit->output_count = output_count;
    
    // 复制输入键名
    if (input_count > 0 && input_keys) {
        unit->input_keys = malloc(input_count * sizeof(char*));
        if (!unit->input_keys) {
            free(unit);
            return NULL;
        }
        
        for (uint32_t i = 0; i < input_count; i++) {
            unit->input_keys[i] = strdup(input_keys[i]);
        }
    }
    
    // 复制输出键名
    if (output_count > 0 && output_keys) {
        unit->output_keys = malloc(output_count * sizeof(char*));
        if (!unit->output_keys) {
            for (uint32_t i = 0; i < input_count; i++) {
                free(unit->input_keys[i]);
            }
            free(unit->input_keys);
            free(unit);
            return NULL;
        }
        
        for (uint32_t i = 0; i < output_count; i++) {
            unit->output_keys[i] = strdup(output_keys[i]);
        }
    }
    
    return unit;
}

// 模型配置结构
typedef struct {
    char* model_path;
    infer_engine_t engine;
    bool engine_loaded;
} model_unit_config_t;

// 模型推理处理函数
static int model_process_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    model_unit_config_t* config = (model_unit_config_t*)context;
    if (!config || !config->engine_loaded) {
        return -1;
    }
    
    // 暂时简化实现：假设单输入单输出
    // 实际实现需要根据模型的具体输入输出规格进行处理
    
    // 获取第一个输入tensor
    if (inputs->count == 0) {
        return -1;
    }
    
    tensor_t* input_tensor = inputs->tensors[0];
    if (!input_tensor) {
        return -1;
    }
    
    // 创建输出tensor（这里需要根据模型规格来确定）
    uint32_t output_dims[] = {1, 1000}; // 假设是1000类分类
    tensor_shape_t output_shape = tensor_shape_create(output_dims, 2);
    tensor_t* output_tensor = malloc(sizeof(tensor_t));
    *output_tensor = tensor_create("model_output", TENSOR_TYPE_FLOAT32, &output_shape, TENSOR_FORMAT_NC);
    
    output_tensor->data = malloc(output_tensor->size);
    output_tensor->owns_data = true;
    
    if (!output_tensor->data) {
        tensor_free(output_tensor);
        free(output_tensor);
        return -1;
    }
    
    // 执行推理（这里应该调用实际的推理引擎）
    // 暂时填充虚拟数据
    float* output_data = (float*)output_tensor->data;
    for (uint32_t i = 0; i < 1000; i++) {
        output_data[i] = (float)(rand() % 1000) / 1000.0f;
    }
    
    // 添加到输出映射表
    if (outputs->count > 0 && outputs->keys[0]) {
        tensor_map_set((tensor_map_t*)outputs, outputs->keys[0], output_tensor);
    } else {
        tensor_map_set((tensor_map_t*)outputs, "model_output", output_tensor);
    }
    
    return 0;
}

processing_unit_t* create_model_unit(const char* name,
                                    const char* model_path,
                                    const char** input_keys, uint32_t input_count,
                                    const char** output_keys, uint32_t output_count) {
    if (!name || !model_path) {
        return NULL;
    }
    
    // 创建模型配置
    model_unit_config_t* config = calloc(1, sizeof(model_unit_config_t));
    if (!config) {
        return NULL;
    }
    
    config->model_path = strdup(model_path);
    config->engine_loaded = false;
    
    // 这里应该加载模型，暂时简化
    // config->engine = infer_engine_create(...);
    // config->engine_loaded = true;
    
    processing_unit_t* unit = create_function_unit(name, model_process_func, config,
                                                  input_keys, input_count,
                                                  output_keys, output_count);
    
    if (!unit) {
        free(config->model_path);
        free(config);
        return NULL;
    }
    
    unit->type = UNIT_TYPE_MODEL;
    unit->config = config;
    
    return unit;
}

// 并行配置结构
typedef struct {
    processing_unit_t** sub_units;
    uint32_t unit_count;
} parallel_unit_config_t;

// 并行处理函数
static int parallel_process_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    parallel_unit_config_t* config = (parallel_unit_config_t*)context;
    if (!config) {
        return -1;
    }
    
    // 暂时简化：顺序执行子单元（实际应该并行执行）
    for (uint32_t i = 0; i < config->unit_count; i++) {
        tensor_map_t* sub_outputs = tensor_map_create(16);
        
        int result = processing_unit_execute(config->sub_units[i], inputs, sub_outputs);
        if (result != 0) {
            tensor_map_destroy(sub_outputs);
            return result;
        }
        
        // 合并输出（简化实现）
        for (uint32_t j = 0; j < sub_outputs->count; j++) {
            char merged_key[128];
            snprintf(merged_key, sizeof(merged_key), "parallel_%u_%s", i, sub_outputs->keys[j]);
            tensor_map_set((tensor_map_t*)outputs, merged_key, sub_outputs->tensors[j]);
        }
        
        tensor_map_destroy(sub_outputs);
    }
    
    return 0;
}

processing_unit_t* create_parallel_unit(const char* name,
                                       processing_unit_t** sub_units,
                                       uint32_t unit_count) {
    if (!name || !sub_units || unit_count == 0) {
        return NULL;
    }
    
    parallel_unit_config_t* config = malloc(sizeof(parallel_unit_config_t));
    if (!config) {
        return NULL;
    }
    
    config->sub_units = malloc(unit_count * sizeof(processing_unit_t*));
    if (!config->sub_units) {
        free(config);
        return NULL;
    }
    
    for (uint32_t i = 0; i < unit_count; i++) {
        config->sub_units[i] = sub_units[i];
    }
    config->unit_count = unit_count;
    
    processing_unit_t* unit = processing_unit_create_base(name, UNIT_TYPE_PARALLEL);
    if (!unit) {
        free(config->sub_units);
        free(config);
        return NULL;
    }
    
    unit->process = parallel_process_func;
    unit->config = config;
    unit->context = config;
    
    return unit;
}

// 条件配置结构
typedef struct {
    process_func_t condition_func;
    processing_unit_t* true_unit;
    processing_unit_t* false_unit;
} conditional_unit_config_t;

// 条件处理函数
static int conditional_process_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    conditional_unit_config_t* config = (conditional_unit_config_t*)context;
    if (!config) {
        return -1;
    }
    
    // 执行条件判断
    tensor_map_t* condition_outputs = tensor_map_create(1);
    int condition_result = config->condition_func(inputs, condition_outputs, NULL);
    
    processing_unit_t* target_unit = NULL;
    if (condition_result == 0) {
        // 检查条件结果（简化：假设条件输出中有一个名为"condition"的tensor）
        tensor_t* condition_tensor = tensor_map_get(condition_outputs, "condition");
        if (condition_tensor && condition_tensor->data) {
            bool condition_value = *((bool*)condition_tensor->data);
            target_unit = condition_value ? config->true_unit : config->false_unit;
        }
    }
    
    tensor_map_destroy(condition_outputs);
    
    if (!target_unit) {
        return -1;
    }
    
    return processing_unit_execute(target_unit, inputs, outputs);
}

processing_unit_t* create_conditional_unit(const char* name,
                                          process_func_t condition_func,
                                          processing_unit_t* true_unit,
                                          processing_unit_t* false_unit) {
    if (!name || !condition_func) {
        return NULL;
    }
    
    conditional_unit_config_t* config = malloc(sizeof(conditional_unit_config_t));
    if (!config) {
        return NULL;
    }
    
    config->condition_func = condition_func;
    config->true_unit = true_unit;
    config->false_unit = false_unit;
    
    processing_unit_t* unit = processing_unit_create_base(name, UNIT_TYPE_CONDITIONAL);
    if (!unit) {
        free(config);
        return NULL;
    }
    
    unit->process = conditional_process_func;
    unit->config = config;
    unit->context = config;
    
    return unit;
}

// 循环配置结构
typedef struct {
    process_func_t loop_condition;
    processing_unit_t* loop_body;
    uint32_t max_iterations;
} loop_unit_config_t;

// 循环处理函数
static int loop_process_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    loop_unit_config_t* config = (loop_unit_config_t*)context;
    if (!config) {
        return -1;
    }
    
    tensor_map_t* current_data = tensor_map_copy(inputs);
    if (!current_data) {
        return -1;
    }
    
    uint32_t iteration = 0;
    while (iteration < config->max_iterations) {
        // 检查循环条件
        tensor_map_t* condition_outputs = tensor_map_create(1);
        int condition_result = config->loop_condition(current_data, condition_outputs, NULL);
        
        bool should_continue = false;
        if (condition_result == 0) {
            tensor_t* condition_tensor = tensor_map_get(condition_outputs, "continue");
            if (condition_tensor && condition_tensor->data) {
                should_continue = *((bool*)condition_tensor->data);
            }
        }
        
        tensor_map_destroy(condition_outputs);
        
        if (!should_continue) {
            break;
        }
        
        // 执行循环体
        tensor_map_t* loop_outputs = tensor_map_create(16);
        int result = processing_unit_execute(config->loop_body, current_data, loop_outputs);
        
        if (result != 0) {
            tensor_map_destroy(loop_outputs);
            tensor_map_destroy(current_data);
            return result;
        }
        
        // 更新当前数据
        tensor_map_destroy(current_data);
        current_data = loop_outputs;
        
        iteration++;
    }
    
    // 复制最终结果到输出
    for (uint32_t i = 0; i < current_data->count; i++) {
        tensor_map_set((tensor_map_t*)outputs, current_data->keys[i], current_data->tensors[i]);
    }
    
    tensor_map_destroy(current_data);
    return 0;
}

processing_unit_t* create_loop_unit(const char* name,
                                   process_func_t loop_condition,
                                   processing_unit_t* loop_body,
                                   uint32_t max_iterations) {
    if (!name || !loop_condition || !loop_body) {
        return NULL;
    }
    
    loop_unit_config_t* config = malloc(sizeof(loop_unit_config_t));
    if (!config) {
        return NULL;
    }
    
    config->loop_condition = loop_condition;
    config->loop_body = loop_body;
    config->max_iterations = max_iterations;
    
    processing_unit_t* unit = processing_unit_create_base(name, UNIT_TYPE_LOOP);
    if (!unit) {
        free(config);
        return NULL;
    }
    
    unit->process = loop_process_func;
    unit->config = config;
    unit->context = config;
    
    return unit;
}

void processing_unit_destroy(processing_unit_t* unit) {
    if (!unit) return;
    
    // 释放输入键名
    for (uint32_t i = 0; i < unit->input_count; i++) {
        free(unit->input_keys[i]);
    }
    free(unit->input_keys);
    
    // 释放输出键名
    for (uint32_t i = 0; i < unit->output_count; i++) {
        free(unit->output_keys[i]);
    }
    free(unit->output_keys);
    
    // 释放类型特定的配置
    switch (unit->type) {
        case UNIT_TYPE_MODEL: {
            model_unit_config_t* config = (model_unit_config_t*)unit->config;
            if (config) {
                free(config->model_path);
                // 这里应该卸载模型
                free(config);
            }
            break;
        }
        case UNIT_TYPE_PARALLEL: {
            parallel_unit_config_t* config = (parallel_unit_config_t*)unit->config;
            if (config) {
                free(config->sub_units);
                free(config);
            }
            break;
        }
        case UNIT_TYPE_CONDITIONAL: {
            conditional_unit_config_t* config = (conditional_unit_config_t*)unit->config;
            free(config);
            break;
        }
        case UNIT_TYPE_LOOP: {
            loop_unit_config_t* config = (loop_unit_config_t*)unit->config;
            free(config);
            break;
        }
        default:
            break;
    }
    
    free(unit);
}

int processing_unit_execute(processing_unit_t* unit, 
                           const tensor_map_t* inputs, 
                           tensor_map_t* outputs) {
    if (!unit || !inputs || !outputs) {
        return -1;
    }
    
    // 检查输入是否满足要求
    for (uint32_t i = 0; i < unit->input_count; i++) {
        if (!tensor_map_has(inputs, unit->input_keys[i])) {
            printf("错误：缺少输入张量 '%s'\n", unit->input_keys[i]);
            return -1;
        }
    }
    
    // 执行处理函数
    if (unit->process) {
        return unit->process(inputs, outputs, unit->context);
    }
    
    return -1;
}

// ================================
// Unified Pipeline 实现
// ================================

unified_pipeline_t* unified_pipeline_create(const char* name) {
    if (!name) {
        return NULL;
    }
    
    unified_pipeline_t* pipeline = calloc(1, sizeof(unified_pipeline_t));
    if (!pipeline) {
        return NULL;
    }
    
    strncpy(pipeline->name, name, sizeof(pipeline->name) - 1);
    pipeline->capacity = 16;
    pipeline->units = calloc(pipeline->capacity, sizeof(processing_unit_t*));
    
    if (!pipeline->units) {
        free(pipeline);
        return NULL;
    }
    
    pipeline->global_map = tensor_map_create(32);
    if (!pipeline->global_map) {
        free(pipeline->units);
        free(pipeline);
        return NULL;
    }
    
    return pipeline;
}

void unified_pipeline_destroy(unified_pipeline_t* pipeline) {
    if (!pipeline) return;
    
    // 销毁所有处理单元
    for (uint32_t i = 0; i < pipeline->unit_count; i++) {
        processing_unit_destroy(pipeline->units[i]);
    }
    
    free(pipeline->units);
    tensor_map_destroy(pipeline->global_map);
    free(pipeline);
}

int unified_pipeline_add_unit(unified_pipeline_t* pipeline, processing_unit_t* unit) {
    if (!pipeline || !unit) {
        return -1;
    }
    
    // 检查是否需要扩容
    if (pipeline->unit_count >= pipeline->capacity) {
        uint32_t new_capacity = pipeline->capacity * 2;
        processing_unit_t** new_units = realloc(pipeline->units, 
                                                new_capacity * sizeof(processing_unit_t*));
        if (!new_units) {
            return -1;
        }
        
        pipeline->units = new_units;
        pipeline->capacity = new_capacity;
        
        // 初始化新分配的空间
        for (uint32_t i = pipeline->unit_count; i < new_capacity; i++) {
            pipeline->units[i] = NULL;
        }
    }
    
    pipeline->units[pipeline->unit_count] = unit;
    pipeline->unit_count++;
    
    return 0;
}

// 获取当前时间（毫秒）
static double get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int unified_pipeline_execute(unified_pipeline_t* pipeline,
                            const tensor_map_t* inputs,
                            tensor_map_t* outputs) {
    if (!pipeline || !inputs || !outputs) {
        return -1;
    }
    
    // 将输入复制到全局映射表
    tensor_map_clear(pipeline->global_map);
    for (uint32_t i = 0; i < inputs->count; i++) {
        tensor_map_set(pipeline->global_map, inputs->keys[i], inputs->tensors[i]);
    }
    
    // 依次执行每个处理单元
    for (uint32_t i = 0; i < pipeline->unit_count; i++) {
        processing_unit_t* unit = pipeline->units[i];
        
        if (pipeline->debug_mode) {
            printf("执行处理单元: %s\n", unit->name);
        }
        
        double start_time = get_current_time_ms();
        
        // 执行处理单元
        int result = processing_unit_execute(unit, pipeline->global_map, pipeline->global_map);
        
        double end_time = get_current_time_ms();
        double execution_time = end_time - start_time;
        
        if (pipeline->debug_mode) {
            printf("处理单元 '%s' 执行完成: 结果=%d, 耗时=%.2fms\n", 
                   unit->name, result, execution_time);
        }
        
        if (result != 0) {
            printf("错误：处理单元 '%s' 执行失败，错误代码: %d\n", unit->name, result);
            return result;
        }
    }
    
    // 将最终结果复制到输出
    for (uint32_t i = 0; i < pipeline->global_map->count; i++) {
        tensor_map_set((tensor_map_t*)outputs, 
                      pipeline->global_map->keys[i], 
                      pipeline->global_map->tensors[i]);
    }
    
    return 0;
}

// 异步执行的线程数据
typedef struct {
    unified_pipeline_t* pipeline;
    tensor_map_t* inputs;
    tensor_map_t* outputs;
    pipeline_completion_callback_t callback;
    void* user_data;
} async_execution_data_t;

// 异步执行的线程函数
static void* async_execution_thread(void* arg) {
    async_execution_data_t* data = (async_execution_data_t*)arg;
    
    int result = unified_pipeline_execute(data->pipeline, data->inputs, data->outputs);
    
    if (data->callback) {
        data->callback(data->pipeline, result, data->user_data);
    }
    
    free(data);
    return NULL;
}

int unified_pipeline_execute_async(unified_pipeline_t* pipeline,
                                  const tensor_map_t* inputs,
                                  tensor_map_t* outputs,
                                  pipeline_completion_callback_t callback,
                                  void* user_data) {
    if (!pipeline || !inputs || !outputs) {
        return -1;
    }
    
    async_execution_data_t* data = malloc(sizeof(async_execution_data_t));
    if (!data) {
        return -1;
    }
    
    data->pipeline = pipeline;
    data->inputs = (tensor_map_t*)inputs;  // 注意：这里应该复制输入数据
    data->outputs = outputs;
    data->callback = callback;
    data->user_data = user_data;
    
    pthread_t thread;
    int result = pthread_create(&thread, NULL, async_execution_thread, data);
    
    if (result != 0) {
        free(data);
        return -1;
    }
    
    // 分离线程，让它自动清理
    pthread_detach(thread);
    
    return 0;
}

int unified_pipeline_set_memory_pool(unified_pipeline_t* pipeline, void* memory_pool) {
    if (!pipeline) {
        return -1;
    }
    
    pipeline->memory_pool = memory_pool;
    pipeline->enable_memory_pool = (memory_pool != NULL);
    
    return 0;
}

void unified_pipeline_set_debug_mode(unified_pipeline_t* pipeline, bool enable) {
    if (pipeline) {
        pipeline->debug_mode = enable;
    }
}

void unified_pipeline_set_unit_callback(unified_pipeline_t* pipeline, unit_execution_callback_t callback) {
    // 暂时简化实现，实际应该存储回调并在执行时调用
    (void)pipeline;
    (void)callback;
}

int unified_pipeline_get_stats(unified_pipeline_t* pipeline,
                              uint32_t* total_units,
                              double* total_execution_time,
                              double* avg_unit_time) {
    if (!pipeline) {
        return -1;
    }
    
    if (total_units) {
        *total_units = pipeline->unit_count;
    }
    
    // 暂时返回虚拟数据，实际应该记录执行统计
    if (total_execution_time) {
        *total_execution_time = 0.0;
    }
    
    if (avg_unit_time) {
        *avg_unit_time = 0.0;
    }
    
    return 0;
}

// ================================
// 常用处理函数实现
// ================================

int image_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* image = tensor_map_get(inputs, "image");
    if (!image) {
        return -1;
    }
    
    // 创建处理后的图像tensor（简化实现）
    tensor_t* processed_image = malloc(sizeof(tensor_t));
    *processed_image = tensor_copy(image);
    
    // 这里应该进行实际的图像预处理：调整大小、归一化等
    
    tensor_map_set((tensor_map_t*)outputs, "processed_image", processed_image);
    
    return 0;
}

int text_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* text = tensor_map_get(inputs, "text");
    if (!text) {
        return -1;
    }
    
    // 创建token tensor（简化实现）
    uint32_t token_dims[] = {1, 512}; // 假设最大512个token
    tensor_shape_t token_shape = tensor_shape_create(token_dims, 2);
    tensor_t* tokens = malloc(sizeof(tensor_t));
    *tokens = tensor_create("tokens", TENSOR_TYPE_INT32, &token_shape, TENSOR_FORMAT_NC);
    
    tokens->data = malloc(tokens->size);
    tokens->owns_data = true;
    
    // 这里应该进行实际的文本预处理：分词、编码等
    // 暂时填充虚拟token
    int32_t* token_data = (int32_t*)tokens->data;
    for (uint32_t i = 0; i < 512; i++) {
        token_data[i] = rand() % 30000; // 假设词汇表大小为30000
    }
    
    tensor_map_set((tensor_map_t*)outputs, "tokens", tokens);
    
    return 0;
}

int audio_preprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* audio = tensor_map_get(inputs, "audio");
    if (!audio) {
        return -1;
    }
    
    // 创建特征tensor（简化实现）
    uint32_t feature_dims[] = {1, 80, 100}; // 假设80个mel频段，100帧
    tensor_shape_t feature_shape = tensor_shape_create(feature_dims, 3);
    tensor_t* features = malloc(sizeof(tensor_t));
    *features = tensor_create("features", TENSOR_TYPE_FLOAT32, &feature_shape, TENSOR_FORMAT_NCHW);
    
    features->data = malloc(features->size);
    features->owns_data = true;
    
    // 这里应该进行实际的音频预处理：重采样、MFCC/Mel谱等
    // 暂时填充虚拟特征
    float* feature_data = (float*)features->data;
    for (uint32_t i = 0; i < 80 * 100; i++) {
        feature_data[i] = (float)(rand() % 1000) / 1000.0f;
    }
    
    tensor_map_set((tensor_map_t*)outputs, "features", features);
    
    return 0;
}

int classification_postprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* logits = tensor_map_get(inputs, "logits");
    if (!logits) {
        return -1;
    }
    
    // 应用softmax并找到最大值（简化实现）
    float* logit_data = (float*)logits->data;
    uint32_t num_classes = logits->shape.dims[1];
    
    // 创建预测结果tensor
    uint32_t pred_dims[] = {1};
    tensor_shape_t pred_shape = tensor_shape_create(pred_dims, 1);
    tensor_t* predictions = malloc(sizeof(tensor_t));
    *predictions = tensor_create("predictions", TENSOR_TYPE_INT32, &pred_shape, TENSOR_FORMAT_N);
    
    predictions->data = malloc(predictions->size);
    predictions->owns_data = true;
    
    // 创建概率tensor
    tensor_t* probabilities = malloc(sizeof(tensor_t));
    *probabilities = tensor_copy(logits);
    probabilities->name = strdup("probabilities");
    
    // 简化的softmax和argmax
    float max_logit = logit_data[0];
    int32_t max_index = 0;
    
    for (uint32_t i = 1; i < num_classes; i++) {
        if (logit_data[i] > max_logit) {
            max_logit = logit_data[i];
            max_index = i;
        }
    }
    
    *((int32_t*)predictions->data) = max_index;
    
    tensor_map_set((tensor_map_t*)outputs, "predictions", predictions);
    tensor_map_set((tensor_map_t*)outputs, "probabilities", probabilities);
    
    return 0;
}

int audio_postprocess_func(const tensor_map_t* inputs, tensor_map_t* outputs, void* context) {
    tensor_t* raw_audio = tensor_map_get(inputs, "raw_audio");
    if (!raw_audio) {
        return -1;
    }
    
    // 创建增强后的音频tensor（简化实现）
    tensor_t* enhanced_audio = malloc(sizeof(tensor_t));
    *enhanced_audio = tensor_copy(raw_audio);
    enhanced_audio->name = strdup("enhanced_audio");
    
    // 这里应该进行实际的音频后处理：降噪、增强等
    // 暂时直接复制数据
    
    tensor_map_set((tensor_map_t*)outputs, "enhanced_audio", enhanced_audio);
    
    return 0;
} 