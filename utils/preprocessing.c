#include "utils/preprocessing.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

/**
 * @brief 预处理操作内部结构
 */
struct PreprocessOp {
    preprocess_params_t params;
    custom_preprocess_func_t custom_func;
    void* context;
    bool enable_cache;
    pthread_mutex_t mutex;
};

/**
 * @brief 预处理管道内部结构
 */
struct PreprocessPipeline {
    preprocess_op_t* ops;
    uint32_t op_count;
    uint32_t capacity;
    uint32_t num_threads;
    pthread_mutex_t mutex;
};

// 全局预处理函数注册表
static custom_preprocess_func_t g_preprocess_funcs[PREPROCESS_CUSTOM + 1] = {0};
static pthread_mutex_t g_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

// 前向声明
static int normalize_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params);
static int resize_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params);
static int rotate_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params);
static int flip_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params);
static int pad_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params);
static int crop_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params);

preprocess_op_t preprocess_op_create(preprocess_type_e type, const preprocess_params_t* params) {
    if (!params || !preprocess_validate_params(type, params)) {
        LOG_ERROR("Invalid preprocessing parameters");
        return NULL;
    }
    
    preprocess_op_t op = calloc(1, sizeof(struct PreprocessOp));
    if (!op) {
        LOG_ERROR("Failed to allocate preprocessing operation");
        return NULL;
    }
    
    op->params = *params;
    op->enable_cache = false;
    
    if (pthread_mutex_init(&op->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize operation mutex");
        free(op);
        return NULL;
    }
    
    LOG_DEBUG("Created preprocessing operation: type=%s", preprocess_type_to_string(type));
    
    return op;
}

void preprocess_op_destroy(preprocess_op_t op) {
    if (!op) return;
    
    pthread_mutex_destroy(&op->mutex);
    free(op);
    LOG_DEBUG("Destroyed preprocessing operation");
}

int preprocess_op_execute(preprocess_op_t op, const Tensor* input, Tensor* output) {
    if (!op || !input || !output) return -1;
    
    pthread_mutex_lock(&op->mutex);
    
    int ret = 0;
    
    // 根据操作类型执行相应的处理
    switch (op->params.type) {
        case PREPROCESS_NORMALIZE:
            ret = normalize_execute(input, output, &op->params);
            break;
            
        case PREPROCESS_RESIZE:
            ret = resize_execute(input, output, &op->params);
            break;
            
        case PREPROCESS_ROTATE:
            ret = rotate_execute(input, output, &op->params);
            break;
            
        case PREPROCESS_FLIP:
            ret = flip_execute(input, output, &op->params);
            break;
            
        case PREPROCESS_PAD:
            ret = pad_execute(input, output, &op->params);
            break;
            
        case PREPROCESS_CROP:
            ret = crop_execute(input, output, &op->params);
            break;
            
        case PREPROCESS_CUSTOM:
            if (op->custom_func) {
                ret = op->custom_func(input, output, &op->params.params.custom, op->context);
            } else {
                LOG_ERROR("Custom preprocessing function not set");
                ret = -1;
            }
            break;
            
        default:
            // 检查全局注册的函数
            pthread_mutex_lock(&g_registry_mutex);
            if (g_preprocess_funcs[op->params.type]) {
                ret = g_preprocess_funcs[op->params.type](input, output, &op->params.params.custom, op->context);
            } else {
                LOG_ERROR("Unsupported preprocessing operation: %s", preprocess_type_to_string(op->params.type));
                ret = -1;
            }
            pthread_mutex_unlock(&g_registry_mutex);
            break;
    }
    
    pthread_mutex_unlock(&op->mutex);
    
    if (ret == 0) {
        LOG_DEBUG("Successfully executed preprocessing operation: %s", 
                  preprocess_type_to_string(op->params.type));
    } else {
        LOG_ERROR("Failed to execute preprocessing operation: %s", 
                  preprocess_type_to_string(op->params.type));
    }
    
    return ret;
}

preprocess_pipeline_t preprocess_pipeline_create(void) {
    preprocess_pipeline_t pipeline = calloc(1, sizeof(struct PreprocessPipeline));
    if (!pipeline) {
        LOG_ERROR("Failed to allocate preprocessing pipeline");
        return NULL;
    }
    
    pipeline->capacity = 8;  // 初始容量
    pipeline->ops = calloc(pipeline->capacity, sizeof(preprocess_op_t));
    if (!pipeline->ops) {
        LOG_ERROR("Failed to allocate operations array");
        free(pipeline);
        return NULL;
    }
    
    pipeline->op_count = 0;
    pipeline->num_threads = 1;
    
    if (pthread_mutex_init(&pipeline->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize pipeline mutex");
        free(pipeline->ops);
        free(pipeline);
        return NULL;
    }
    
    LOG_DEBUG("Created preprocessing pipeline");
    
    return pipeline;
}

void preprocess_pipeline_destroy(preprocess_pipeline_t pipeline) {
    if (!pipeline) return;
    
    // 销毁所有操作
    for (uint32_t i = 0; i < pipeline->op_count; i++) {
        preprocess_op_destroy(pipeline->ops[i]);
    }
    
    free(pipeline->ops);
    pthread_mutex_destroy(&pipeline->mutex);
    free(pipeline);
    
    LOG_DEBUG("Destroyed preprocessing pipeline");
}

int preprocess_pipeline_add_op(preprocess_pipeline_t pipeline, preprocess_op_t op) {
    if (!pipeline || !op) return -1;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    // 检查是否需要扩容
    if (pipeline->op_count >= pipeline->capacity) {
        uint32_t new_capacity = pipeline->capacity * 2;
        preprocess_op_t* new_ops = realloc(pipeline->ops, new_capacity * sizeof(preprocess_op_t));
        if (!new_ops) {
            LOG_ERROR("Failed to expand operations array");
            pthread_mutex_unlock(&pipeline->mutex);
            return -1;
        }
        
        pipeline->ops = new_ops;
        pipeline->capacity = new_capacity;
    }
    
    pipeline->ops[pipeline->op_count] = op;
    pipeline->op_count++;
    
    pthread_mutex_unlock(&pipeline->mutex);
    
    LOG_DEBUG("Added operation to pipeline, count: %u", pipeline->op_count);
    
    return 0;
}

int preprocess_pipeline_remove_op(preprocess_pipeline_t pipeline, uint32_t index) {
    if (!pipeline || index >= pipeline->op_count) return -1;
    
    pthread_mutex_lock(&pipeline->mutex);
    
    // 销毁要移除的操作
    preprocess_op_destroy(pipeline->ops[index]);
    
    // 移动后续操作
    if (index < pipeline->op_count - 1) {
        memmove(&pipeline->ops[index], &pipeline->ops[index + 1],
                (pipeline->op_count - index - 1) * sizeof(preprocess_op_t));
    }
    
    pipeline->op_count--;
    
    pthread_mutex_unlock(&pipeline->mutex);
    
    LOG_DEBUG("Removed operation from pipeline, count: %u", pipeline->op_count);
    
    return 0;
}

int preprocess_pipeline_execute(preprocess_pipeline_t pipeline, const Tensor* input, Tensor* output) {
    if (!pipeline || !input || !output) return -1;
    
    if (pipeline->op_count == 0) {
        // 没有操作，直接复制输入到输出
        output->shape = input->shape;
        output->dtype = input->dtype;
        
        size_t total_elements = 1;
        for (uint32_t i = 0; i < input->shape.ndim; i++) {
            total_elements *= input->shape.dims[i];
        }
        
        size_t element_size = 1;
        switch (input->dtype) {
            case TENSOR_TYPE_FLOAT32: case TENSOR_TYPE_INT32: element_size = 4; break;
            case TENSOR_TYPE_FLOAT64: case TENSOR_TYPE_INT64: element_size = 8; break;
            case TENSOR_TYPE_FLOAT16: case TENSOR_TYPE_INT16: element_size = 2; break;
            default: element_size = 1; break;
        }
        
        size_t data_size = total_elements * element_size;
        output->data = malloc(data_size);
        if (!output->data) return -1;
        
        memcpy(output->data, input->data, data_size);
        return 0;
    }
    
    // 为中间结果分配缓冲区
    Tensor* buffers = malloc((pipeline->op_count + 1) * sizeof(Tensor));
    if (!buffers) {
        LOG_ERROR("Failed to allocate intermediate buffers");
        return -1;
    }
    
    // 第一个缓冲区是输入
    buffers[0] = *input;
    
    // 执行每个操作
    int ret = 0;
    for (uint32_t i = 0; i < pipeline->op_count; i++) {
        const Tensor* op_input = &buffers[i];
        Tensor* op_output = &buffers[i + 1];
        
        ret = preprocess_op_execute(pipeline->ops[i], op_input, op_output);
        if (ret != 0) {
            LOG_ERROR("Failed to execute operation %u in pipeline", i);
            break;
        }
    }
    
    if (ret == 0) {
        // 复制最终结果到输出
        *output = buffers[pipeline->op_count];
    }
    
    // 释放中间缓冲区（除了第一个和最后一个）
    for (uint32_t i = 1; i < pipeline->op_count; i++) {
        if (buffers[i].data && buffers[i].data != input->data) {
            free(buffers[i].data);
        }
    }
    
    free(buffers);
    
    return ret;
}

uint32_t preprocess_pipeline_get_op_count(preprocess_pipeline_t pipeline) {
    if (!pipeline) return 0;
    
    pthread_mutex_lock(&pipeline->mutex);
    uint32_t count = pipeline->op_count;
    pthread_mutex_unlock(&pipeline->mutex);
    
    return count;
}

preprocess_op_t preprocess_pipeline_get_op(preprocess_pipeline_t pipeline, uint32_t index) {
    if (!pipeline || index >= pipeline->op_count) return NULL;
    
    pthread_mutex_lock(&pipeline->mutex);
    preprocess_op_t op = pipeline->ops[index];
    pthread_mutex_unlock(&pipeline->mutex);
    
    return op;
}

preprocess_op_t preprocess_op_create_custom(custom_preprocess_func_t func, const void* params, 
                                        size_t params_size, void* context) {
    if (!func) return NULL;
    
    preprocess_params_t pp_params = {0};
    pp_params.type = PREPROCESS_CUSTOM;
    pp_params.params.custom.custom_data = malloc(params_size);
    if (!pp_params.params.custom.custom_data) return NULL;
    
    memcpy(pp_params.params.custom.custom_data, params, params_size);
    pp_params.params.custom.data_size = params_size;
    
    preprocess_op_t op = preprocess_op_create(PREPROCESS_CUSTOM, &pp_params);
    if (op) {
        op->custom_func = func;
        op->context = context;
    }
    
    return op;
}

int preprocess_register_op(preprocess_type_e type, custom_preprocess_func_t func) {
    if (type <= PREPROCESS_UNKNOWN || type >= PREPROCESS_CUSTOM || !func) return -1;
    
    pthread_mutex_lock(&g_registry_mutex);
    g_preprocess_funcs[type] = func;
    pthread_mutex_unlock(&g_registry_mutex);
    
    LOG_DEBUG("Registered preprocessing operation: %s", preprocess_type_to_string(type));
    
    return 0;
}

const char* preprocess_type_to_string(preprocess_type_e type) {
    switch (type) {
        case PREPROCESS_NORMALIZE: return "normalize";
        case PREPROCESS_STANDARDIZE: return "standardize";
        case PREPROCESS_QUANTIZE: return "quantize";
        case PREPROCESS_DEQUANTIZE: return "dequantize";
        case PREPROCESS_CAST: return "cast";
        case PREPROCESS_TRANSPOSE: return "transpose";
        case PREPROCESS_RESHAPE: return "reshape";
        case PREPROCESS_PAD: return "pad";
        case PREPROCESS_CROP: return "crop";
        case PREPROCESS_RESIZE: return "resize";
        case PREPROCESS_ROTATE: return "rotate";
        case PREPROCESS_FLIP: return "flip";
        case PREPROCESS_COLOR_CONVERT: return "color_convert";
        case PREPROCESS_BRIGHTNESS: return "brightness";
        case PREPROCESS_CONTRAST: return "contrast";
        case PREPROCESS_GAMMA: return "gamma";
        case PREPROCESS_BLUR: return "blur";
        case PREPROCESS_SHARPEN: return "sharpen";
        case PREPROCESS_EDGE_DETECT: return "edge_detect";
        case PREPROCESS_MORPHOLOGY: return "morphology";
        case PREPROCESS_HISTOGRAM_EQ: return "histogram_eq";
        case PREPROCESS_RESAMPLE: return "resample";
        case PREPROCESS_AMPLITUDE_SCALE: return "amplitude_scale";
        case PREPROCESS_NOISE_REDUCTION: return "noise_reduction";
        case PREPROCESS_FILTER: return "filter";
        case PREPROCESS_WINDOWING: return "windowing";
        case PREPROCESS_FFT: return "fft";
        case PREPROCESS_MFCC: return "mfcc";
        case PREPROCESS_SPECTROGRAM: return "spectrogram";
        case PREPROCESS_PITCH_SHIFT: return "pitch_shift";
        case PREPROCESS_TIME_STRETCH: return "time_stretch";
        case PREPROCESS_TOKENIZE: return "tokenize";
        case PREPROCESS_ENCODE: return "encode";
        case PREPROCESS_DECODE: return "decode";
        case PREPROCESS_EMBEDDING: return "embedding";
        case PREPROCESS_SEQUENCE_PAD: return "sequence_pad";
        case PREPROCESS_SEQUENCE_TRUNCATE: return "sequence_truncate";
        case PREPROCESS_MASK: return "mask";
        case PREPROCESS_DOWNSAMPLE: return "downsample";
        case PREPROCESS_UPSAMPLE: return "upsample";
        case PREPROCESS_OUTLIER_REMOVAL: return "outlier_removal";
        case PREPROCESS_SMOOTH: return "smooth";
        case PREPROCESS_NORMAL_ESTIMATION: return "normal_estimation";
        case PREPROCESS_REGISTRATION: return "registration";
        case PREPROCESS_SEGMENTATION: return "segmentation";
        case PREPROCESS_CUSTOM: return "custom";
        default: return "unknown";
    }
}

preprocess_type_e preprocess_type_from_string(const char* type_str) {
    if (!type_str) return PREPROCESS_UNKNOWN;
    
    if (strcmp(type_str, "normalize") == 0) return PREPROCESS_NORMALIZE;
    if (strcmp(type_str, "standardize") == 0) return PREPROCESS_STANDARDIZE;
    if (strcmp(type_str, "quantize") == 0) return PREPROCESS_QUANTIZE;
    if (strcmp(type_str, "dequantize") == 0) return PREPROCESS_DEQUANTIZE;
    if (strcmp(type_str, "cast") == 0) return PREPROCESS_CAST;
    if (strcmp(type_str, "transpose") == 0) return PREPROCESS_TRANSPOSE;
    if (strcmp(type_str, "reshape") == 0) return PREPROCESS_RESHAPE;
    if (strcmp(type_str, "pad") == 0) return PREPROCESS_PAD;
    if (strcmp(type_str, "crop") == 0) return PREPROCESS_CROP;
    if (strcmp(type_str, "resize") == 0) return PREPROCESS_RESIZE;
    if (strcmp(type_str, "rotate") == 0) return PREPROCESS_ROTATE;
    if (strcmp(type_str, "flip") == 0) return PREPROCESS_FLIP;
    if (strcmp(type_str, "color_convert") == 0) return PREPROCESS_COLOR_CONVERT;
    if (strcmp(type_str, "brightness") == 0) return PREPROCESS_BRIGHTNESS;
    if (strcmp(type_str, "contrast") == 0) return PREPROCESS_CONTRAST;
    if (strcmp(type_str, "gamma") == 0) return PREPROCESS_GAMMA;
    if (strcmp(type_str, "blur") == 0) return PREPROCESS_BLUR;
    if (strcmp(type_str, "sharpen") == 0) return PREPROCESS_SHARPEN;
    if (strcmp(type_str, "edge_detect") == 0) return PREPROCESS_EDGE_DETECT;
    if (strcmp(type_str, "morphology") == 0) return PREPROCESS_MORPHOLOGY;
    if (strcmp(type_str, "histogram_eq") == 0) return PREPROCESS_HISTOGRAM_EQ;
    if (strcmp(type_str, "resample") == 0) return PREPROCESS_RESAMPLE;
    if (strcmp(type_str, "amplitude_scale") == 0) return PREPROCESS_AMPLITUDE_SCALE;
    if (strcmp(type_str, "noise_reduction") == 0) return PREPROCESS_NOISE_REDUCTION;
    if (strcmp(type_str, "filter") == 0) return PREPROCESS_FILTER;
    if (strcmp(type_str, "windowing") == 0) return PREPROCESS_WINDOWING;
    if (strcmp(type_str, "fft") == 0) return PREPROCESS_FFT;
    if (strcmp(type_str, "mfcc") == 0) return PREPROCESS_MFCC;
    if (strcmp(type_str, "spectrogram") == 0) return PREPROCESS_SPECTROGRAM;
    if (strcmp(type_str, "pitch_shift") == 0) return PREPROCESS_PITCH_SHIFT;
    if (strcmp(type_str, "time_stretch") == 0) return PREPROCESS_TIME_STRETCH;
    if (strcmp(type_str, "tokenize") == 0) return PREPROCESS_TOKENIZE;
    if (strcmp(type_str, "encode") == 0) return PREPROCESS_ENCODE;
    if (strcmp(type_str, "decode") == 0) return PREPROCESS_DECODE;
    if (strcmp(type_str, "embedding") == 0) return PREPROCESS_EMBEDDING;
    if (strcmp(type_str, "sequence_pad") == 0) return PREPROCESS_SEQUENCE_PAD;
    if (strcmp(type_str, "sequence_truncate") == 0) return PREPROCESS_SEQUENCE_TRUNCATE;
    if (strcmp(type_str, "mask") == 0) return PREPROCESS_MASK;
    if (strcmp(type_str, "downsample") == 0) return PREPROCESS_DOWNSAMPLE;
    if (strcmp(type_str, "upsample") == 0) return PREPROCESS_UPSAMPLE;
    if (strcmp(type_str, "outlier_removal") == 0) return PREPROCESS_OUTLIER_REMOVAL;
    if (strcmp(type_str, "smooth") == 0) return PREPROCESS_SMOOTH;
    if (strcmp(type_str, "normal_estimation") == 0) return PREPROCESS_NORMAL_ESTIMATION;
    if (strcmp(type_str, "registration") == 0) return PREPROCESS_REGISTRATION;
    if (strcmp(type_str, "segmentation") == 0) return PREPROCESS_SEGMENTATION;
    if (strcmp(type_str, "custom") == 0) return PREPROCESS_CUSTOM;
    
    return PREPROCESS_UNKNOWN;
}

bool preprocess_validate_params(preprocess_type_e type, const preprocess_params_t* params) {
    if (!params) return false;
    
    switch (type) {
        case PREPROCESS_NORMALIZE:
            return params->params.normalize.channels > 0 && 
                   params->params.normalize.channels <= 4;
                   
        case PREPROCESS_RESIZE:
            return params->params.resize.width > 0 && 
                   params->params.resize.height > 0;
                   
        case PREPROCESS_PAD:
            return true; // 所有填充参数都是有效的
            
        case PREPROCESS_CROP:
            return params->params.crop.width > 0 && 
                   params->params.crop.height > 0;
                   
        case PREPROCESS_CUSTOM:
            return params->params.custom.custom_data != NULL;
            
        default:
            return true; // 其他操作类型暂时都认为是有效的
    }
}

// 具体的预处理操作实现

static int normalize_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params) {
    if (!input || !output || !params) return -1;
    
    uint32_t channels = params->params.normalize.channels;
    float mean[4] = {0};
    float std[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    
    for (uint32_t i = 0; i < channels; i++) {
        mean[i] = params->params.normalize.mean[i];
        std[i] = params->params.normalize.std[i];
    }
    
    // 简单的归一化实现
    size_t total_elements = 1;
    for (uint32_t i = 0; i < input->shape.ndim; i++) {
        total_elements *= input->shape.dims[i];
    }
    
    if (input->dtype == TENSOR_TYPE_FLOAT32) {
        float* input_data = (float*)input->data;
        float* output_data = (float*)output->data;
        
        for (size_t i = 0; i < total_elements; i++) {
            uint32_t channel = i % channels;
            output_data[i] = (input_data[i] - mean[channel]) / std[channel];
        }
    } else {
        LOG_ERROR("Unsupported data type for normalization");
        return -1;
    }
    
    return 0;
}

static int resize_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params) {
    if (!input || !output || !params) return -1;
    
    uint32_t width = params->params.resize.width;
    uint32_t height = params->params.resize.height;
    interpolation_method_e method = params->params.resize.method;
    
    // 简单的最近邻插值实现
    if (input->dtype == TENSOR_TYPE_FLOAT32) {
        float* input_data = (float*)input->data;
        float* output_data = (float*)output->data;
        
        // 假设输入是NHWC格式的图像
        uint32_t input_height = input->shape.dims[1];
        uint32_t input_width = input->shape.dims[2];
        uint32_t channels = input->shape.dims[3];
        
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t src_x = (x * input_width) / width;
                uint32_t src_y = (y * input_height) / height;
                
                for (uint32_t c = 0; c < channels; c++) {
                    uint32_t src_idx = (src_y * input_width + src_x) * channels + c;
                    uint32_t dst_idx = (y * width + x) * channels + c;
                    output_data[dst_idx] = input_data[src_idx];
                }
            }
        }
    } else {
        LOG_ERROR("Unsupported data type for resize");
        return -1;
    }
    
    return 0;
}

static int rotate_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params) {
    (void)input;
    (void)output;
    (void)params;
    // TODO: 实现旋转操作
    return 0;
}

static int flip_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params) {
    (void)input;
    (void)output;
    (void)params;
    // TODO: 实现翻转操作
    return 0;
}

static int pad_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params) {
    (void)input;
    (void)output;
    (void)params;
    // TODO: 实现填充操作
    return 0;
}

static int crop_execute(const Tensor* input, Tensor* output, const preprocess_params_t* params) {
    (void)input;
    (void)output;
    (void)params;
    // TODO: 实现裁剪操作
    return 0;
} 