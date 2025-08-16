#include "core/model_parser.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/**
 * @brief 模型解析器内部结构
 */
struct ModelParser {
    pthread_mutex_t mutex;
    model_format_e format;
    char* model_path;
    void* parser_data;
};

model_parser_t model_parser_create(void) {
    model_parser_t parser = calloc(1, sizeof(struct ModelParser));
    if (!parser) {
        LOG_ERROR("Failed to allocate model parser");
        return NULL;
    }
    
    if (pthread_mutex_init(&parser->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize parser mutex");
        free(parser);
        return NULL;
    }
    
    LOG_DEBUG("Created model parser");
    
    return parser;
}

void model_parser_destroy(model_parser_t parser) {
    if (!parser) return;
    
    pthread_mutex_destroy(&parser->mutex);
    free(parser->model_path);
    free(parser);
    
    LOG_DEBUG("Destroyed model parser");
}

model_format_e model_parser_detect_format(const char* model_path) {
    if (!model_path) return MODEL_FORMAT_UNKNOWN;
    
    // 简单的文件扩展名检测
    const char* ext = strrchr(model_path, '.');
    if (!ext) return MODEL_FORMAT_UNKNOWN;
    
    if (strcasecmp(ext, ".onnx") == 0) {
        LOG_INFO("Detected ONNX model format");
        return MODEL_FORMAT_ONNX;
    } else if (strcasecmp(ext, ".rknn") == 0) {
        LOG_INFO("Detected RKNN model format");
        return MODEL_FORMAT_RKNN;
    } else if (strcasecmp(ext, ".xml") == 0 || strcasecmp(ext, ".bin") == 0) {
        LOG_INFO("Detected OpenVINO model format");
        return MODEL_FORMAT_OPENVINO;
    } else if (strcasecmp(ext, ".engine") == 0 || strcasecmp(ext, ".trt") == 0) {
        LOG_INFO("Detected TensorRT model format");
        return MODEL_FORMAT_TENSORRT;
    } else if (strcasecmp(ext, ".pt") == 0 || strcasecmp(ext, ".pth") == 0) {
        LOG_INFO("Detected PyTorch model format");
        return MODEL_FORMAT_PYTORCH;
    } else if (strcasecmp(ext, ".pb") == 0 || strcasecmp(ext, ".savedmodel") == 0) {
        LOG_INFO("Detected TensorFlow model format");
        return MODEL_FORMAT_TENSORFLOW;
    } else if (strcasecmp(ext, ".tflite") == 0) {
        LOG_INFO("Detected TensorFlow Lite model format");
        return MODEL_FORMAT_TFLITE;
    }
    
    LOG_WARN("Unknown model format: %s", model_path);
    return MODEL_FORMAT_UNKNOWN;
}

int model_parser_parse_metadata(model_parser_t parser, const char* model_path, model_metadata_t* metadata) {
    if (!parser || !model_path || !metadata) return -1;
    
    pthread_mutex_lock(&parser->mutex);
    
    // 检测模型格式
    model_format_e format = model_parser_detect_format(model_path);
    if (format == MODEL_FORMAT_UNKNOWN) {
        LOG_ERROR("Failed to detect model format: %s", model_path);
        pthread_mutex_unlock(&parser->mutex);
        return -1;
    }
    
    // 解析模型元信息
    memset(metadata, 0, sizeof(model_metadata_t));
    
    // 设置基本信息
    metadata->name = strdup("Unknown Model");
    metadata->version = strdup("1.0.0");
    metadata->description = strdup("Model loaded by Modyn");
    metadata->author = strdup("Unknown");
    metadata->license = strdup("Unknown");
    metadata->format = format;
    
    // 根据格式设置推荐后端
    switch (format) {
        case MODEL_FORMAT_ONNX:
            metadata->preferred_backend = INFER_BACKEND_ONNX;
            break;
        case MODEL_FORMAT_RKNN:
            metadata->preferred_backend = INFER_BACKEND_RKNN;
            break;
        case MODEL_FORMAT_OPENVINO:
            metadata->preferred_backend = INFER_BACKEND_OPENVINO;
            break;
        case MODEL_FORMAT_TENSORRT:
            metadata->preferred_backend = INFER_BACKEND_TENSORRT;
            break;
        case MODEL_FORMAT_PYTORCH:
            metadata->preferred_backend = INFER_BACKEND_ONNX; // 使用ONNX作为PyTorch的后端
            break;
        case MODEL_FORMAT_TENSORFLOW:
        case MODEL_FORMAT_TFLITE:
            metadata->preferred_backend = INFER_BACKEND_ONNX; // 使用ONNX作为TensorFlow的后端
            break;
        default:
            metadata->preferred_backend = INFER_BACKEND_DUMMY;
            break;
    }
    
    // 获取文件大小
    FILE* file = fopen(model_path, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        metadata->model_size = ftell(file);
        fclose(file);
    }
    
    // 估算所需内存（简化实现）
    metadata->memory_required = metadata->model_size * 2;
    metadata->supports_batching = true;
    metadata->max_batch_size = 32;
    
    pthread_mutex_unlock(&parser->mutex);
    
    LOG_INFO("Parsed model metadata: %s, format: %s", model_path, model_format_to_string(format));
    
    return 0;
}

int model_parser_parse_inputs(model_parser_t parser, const char* model_path, 
                             model_io_spec_t** input_specs, uint32_t* input_count) {
    if (!parser || !model_path || !input_specs || !input_count) return -1;
    
    pthread_mutex_lock(&parser->mutex);
    
    // 简化实现：创建默认输入规格
    *input_count = 1;
    *input_specs = calloc(1, sizeof(model_io_spec_t));
    if (!*input_specs) {
        pthread_mutex_unlock(&parser->mutex);
        return -1;
    }
    
    model_io_spec_t* spec = *input_specs;
    spec->name = strdup("input");
    spec->shape.ndim = 4;
    spec->shape.dims[0] = 1;   // batch
    spec->shape.dims[1] = 224; // height
    spec->shape.dims[2] = 224; // width
    spec->shape.dims[3] = 3;   // channels
    spec->data_type = TENSOR_TYPE_FLOAT32;
    spec->is_dynamic = false;
    spec->description = strdup("Default input tensor");
    
    pthread_mutex_unlock(&parser->mutex);
    
    LOG_DEBUG("Parsed input specifications for: %s", model_path);
    
    return 0;
}

int model_parser_parse_outputs(model_parser_t parser, const char* model_path,
                              model_io_spec_t** output_specs, uint32_t* output_count) {
    if (!parser || !model_path || !output_specs || !output_count) return -1;
    
    pthread_mutex_lock(&parser->mutex);
    
    // 简化实现：创建默认输出规格
    *output_count = 1;
    *output_specs = calloc(1, sizeof(model_io_spec_t));
    if (!*output_specs) {
        pthread_mutex_unlock(&parser->mutex);
        return -1;
    }
    
    model_io_spec_t* spec = *output_specs;
    spec->name = strdup("output");
    spec->shape.ndim = 2;
    spec->shape.dims[0] = 1;    // batch
    spec->shape.dims[1] = 1000; // classes
    spec->data_type = TENSOR_TYPE_FLOAT32;
    spec->is_dynamic = false;
    spec->description = strdup("Default output tensor");
    
    pthread_mutex_unlock(&parser->mutex);
    
    LOG_DEBUG("Parsed output specifications for: %s", model_path);
    
    return 0;
}

bool model_parser_validate_compatibility(model_parser_t parser, const char* model_path, 
                                        InferBackendType backend) {
    if (!parser || !model_path) return false;
    
    model_format_e format = model_parser_detect_format(model_path);
    
    // 简单的兼容性检查
    switch (format) {
        case MODEL_FORMAT_ONNX:
            return backend == INFER_BACKEND_ONNX || backend == INFER_BACKEND_DUMMY;
        case MODEL_FORMAT_RKNN:
            return backend == INFER_BACKEND_RKNN;
        case MODEL_FORMAT_OPENVINO:
            return backend == INFER_BACKEND_OPENVINO || backend == INFER_BACKEND_DUMMY;
        case MODEL_FORMAT_TENSORRT:
            return backend == INFER_BACKEND_TENSORRT;
        case MODEL_FORMAT_PYTORCH:
            return backend == INFER_BACKEND_ONNX || backend == INFER_BACKEND_DUMMY;
        case MODEL_FORMAT_TENSORFLOW:
        case MODEL_FORMAT_TFLITE:
            return backend == INFER_BACKEND_ONNX || backend == INFER_BACKEND_DUMMY;
        default:
            return backend == INFER_BACKEND_DUMMY;
    }
}

int model_parser_suggest_config(model_parser_t parser, const model_metadata_t* metadata, 
                               InferBackendType backend, InferEngineConfig* config) {
    if (!parser || !metadata || !config) return -1;
    
    // 设置默认配置
    memset(config, 0, sizeof(InferEngineConfig));
    config->backend = backend;
    config->device_id = 0;
    config->num_threads = 4;
    config->enable_fp16 = false;
    config->enable_int8 = false;
    config->custom_config = NULL;
    
    // 根据后端调整配置
    switch (backend) {
        case INFER_BACKEND_DUMMY:
            config->num_threads = 4;
            break;
        case INFER_BACKEND_TENSORRT:
            config->device_id = 0;
            config->num_threads = 1;
            config->enable_fp16 = true;
            break;
        case INFER_BACKEND_RKNN:
            config->device_id = 0;
            config->num_threads = 1;
            config->enable_int8 = true;
            break;
        case INFER_BACKEND_ONNX:
            config->num_threads = 4;
            break;
        case INFER_BACKEND_OPENVINO:
            config->num_threads = 4;
            break;
        default:
            break;
    }
    
    LOG_DEBUG("Suggested config for backend %d: threads=%d, fp16=%d, int8=%d", 
              backend, config->num_threads, config->enable_fp16, config->enable_int8);
    
    return 0;
}

void model_metadata_free(model_metadata_t* metadata) {
    if (!metadata) return;
    
    free(metadata->name);
    free(metadata->version);
    free(metadata->description);
    free(metadata->author);
    free(metadata->license);
    free(metadata->inputs);
    free(metadata->outputs);
    free(metadata->custom_metadata);
    
    memset(metadata, 0, sizeof(model_metadata_t));
}

void model_io_specs_free(model_io_spec_t* specs, uint32_t count) {
    if (!specs) return;
    
    for (uint32_t i = 0; i < count; i++) {
        free(specs[i].name);
        free(specs[i].description);
    }
    
    free(specs);
}

const char* model_format_to_string(model_format_e format) {
    switch (format) {
        case MODEL_FORMAT_ONNX: return "ONNX";
        case MODEL_FORMAT_RKNN: return "RKNN";
        case MODEL_FORMAT_OPENVINO: return "OpenVINO";
        case MODEL_FORMAT_TENSORRT: return "TensorRT";
        case MODEL_FORMAT_PYTORCH: return "PyTorch";
        case MODEL_FORMAT_TENSORFLOW: return "TensorFlow";
        case MODEL_FORMAT_TFLITE: return "TensorFlow Lite";
        default: return "Unknown";
    }
}

model_format_e model_format_from_string(const char* format_str) {
    if (!format_str) return MODEL_FORMAT_UNKNOWN;
    
    if (strcasecmp(format_str, "onnx") == 0) return MODEL_FORMAT_ONNX;
    if (strcasecmp(format_str, "rknn") == 0) return MODEL_FORMAT_RKNN;
    if (strcasecmp(format_str, "openvino") == 0) return MODEL_FORMAT_OPENVINO;
    if (strcasecmp(format_str, "tensorrt") == 0) return MODEL_FORMAT_TENSORRT;
    if (strcasecmp(format_str, "pytorch") == 0) return MODEL_FORMAT_PYTORCH;
    if (strcasecmp(format_str, "tensorflow") == 0) return MODEL_FORMAT_TENSORFLOW;
    if (strcasecmp(format_str, "tflite") == 0) return MODEL_FORMAT_TFLITE;
    
    return MODEL_FORMAT_UNKNOWN;
} 