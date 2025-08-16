#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "core/model_manager.h"
#include "core/model_parser.h"
#include "core/memory_pool.h"
#include "core/multimodal.h"
#include "core/instance_manager.h"
#include "core/plugin_factory.h"
#include "utils/preprocessing.h"
#include "utils/logger.h"

/**
 * @brief 多模态推理示例
 * 
 * 此示例展示了如何使用Modyn系统进行多模态推理：
 * 1. 模型解析和加载
 * 2. 内存池管理
 * 3. 多模态数据处理
 * 4. 实例管理和权重共享
 * 5. 预处理管道
 * 6. 插件系统
 */
int main(int argc, char* argv[]) {
    int ret = 0;
    
    // 初始化日志系统
    LOG_INFO("Starting multimodal inference example");
    
    // 创建内存池
    memory_pool_config_t pool_config = {
        .type = MEMORY_POOL_CPU,
        .initial_size = 1024 * 1024 * 100,  // 100MB
        .max_size = 1024 * 1024 * 1024,     // 1GB
        .grow_size = 1024 * 1024 * 10,      // 10MB
        .alignment = 32,
        .strategy = MEMORY_ALLOC_BEST_FIT,
        .enable_tracking = true,
        .enable_debug = false,
        .external_memory = NULL,
        .external_size = 0
    };
    
    memory_pool_t memory_pool = memory_pool_create(&pool_config);
    if (!memory_pool) {
        printf("❌ 创建内存池失败\n");
        return -1;
    }
    printf("✅ 内存池创建成功\n");
    
    // 2. 创建模型解析器
    ModelParser parser = model_parser_create();
    if (!parser) {
        LOG_ERROR("Failed to create model parser");
        ret = -1;
        goto cleanup_memory_pool;
    }
    
    // 3. 解析模型（如果有模型文件）
    const char* model_path = "model.onnx";
    if (access(model_path, F_OK) == 0) {
        ModelMetadata metadata;
        if (model_parser_parse_metadata(parser, model_path, &metadata) == 0) {
            LOG_INFO("Model parsed successfully:");
            LOG_INFO("  Name: %s", metadata.name);
            LOG_INFO("  Format: %s", model_format_to_string(metadata.format));
            LOG_INFO("  Preferred backend: %s", infer_engine_get_backend_name(metadata.preferred_backend));
            LOG_INFO("  Memory required: %zu bytes", metadata.memory_required);
            LOG_INFO("  Supports batching: %s", metadata.supports_batching ? "Yes" : "No");
            
            model_metadata_free(&metadata);
        } else {
            LOG_WARN("Failed to parse model metadata");
        }
    }
    
    // 4. 创建实例管理器
    InstanceManager instance_manager = instance_manager_create(memory_pool);
    if (!instance_manager) {
        LOG_ERROR("Failed to create instance manager");
        ret = -1;
        goto cleanup_parser;
    }
    
    // 5. 创建插件工厂
    PluginFactory plugin_factory = plugin_factory_create();
    if (!plugin_factory) {
        LOG_ERROR("Failed to create plugin factory");
        ret = -1;
        goto cleanup_instance_manager;
    }
    
    // 添加插件搜索路径
    plugin_factory_add_search_path(plugin_factory, "./plugins");
    plugin_factory_add_search_path(plugin_factory, "/usr/local/lib/modyn/plugins");
    
    // 发现可用插件
    int plugin_count = plugin_factory_discover(plugin_factory, NULL, NULL);
    LOG_INFO("Discovered %d plugins", plugin_count);
    
    // 6. 创建多模态数据
    MultiModalData* input_data = multimodal_data_create(3);
    if (!input_data) {
        LOG_ERROR("Failed to create multimodal data");
        ret = -1;
        goto cleanup_plugin_factory;
    }
    
    // 创建文本模态数据
    const char* text_content = "This is a sample text for multimodal inference.";
    ModalityData* text_data = modality_data_create(
        MODALITY_TEXT, 
        DATA_FORMAT_UTF8, 
        text_content, 
        strlen(text_content)
    );
    
    if (text_data) {
        text_data->timestamp = 1234567890;
        text_data->sequence_id = 1;
        text_data->source_id = strdup("text_input");
        text_data->metadata = strdup("{\"language\": \"en\", \"encoding\": \"utf-8\"}");
        
        multimodal_data_add(input_data, text_data);
        LOG_INFO("Added text modality data");
    }
    
    // 创建图像模态数据（模拟RGB图像）
    uint32_t image_width = 224, image_height = 224, channels = 3;
    size_t image_size = image_width * image_height * channels;
    uint8_t* image_buffer = malloc(image_size);
    
    if (image_buffer) {
        // 填充模拟图像数据
        for (size_t i = 0; i < image_size; i++) {
            image_buffer[i] = (uint8_t)(i % 256);
        }
        
        ModalityData* image_data = modality_data_create(
            MODALITY_IMAGE, 
            DATA_FORMAT_RGB, 
            image_buffer, 
            image_size
        );
        
        if (image_data) {
            image_data->shape.dims[0] = 1;  // batch
            image_data->shape.dims[1] = channels;
            image_data->shape.dims[2] = image_height;
            image_data->shape.dims[3] = image_width;
            image_data->shape.ndim = 4;
            image_data->data_type = TENSOR_UINT8;
            image_data->timestamp = 1234567891;
            image_data->sequence_id = 2;
            image_data->source_id = strdup("camera_input");
            image_data->metadata = strdup("{\"width\": 224, \"height\": 224, \"channels\": 3}");
            
            multimodal_data_add(input_data, image_data);
            LOG_INFO("Added image modality data");
        }
        
        free(image_buffer);
    }
    
    // 创建音频模态数据（模拟PCM音频）
    uint32_t sample_rate = 16000;
    uint32_t duration_ms = 1000;
    uint32_t audio_samples = sample_rate * duration_ms / 1000;
    size_t audio_size = audio_samples * sizeof(int16_t);
    int16_t* audio_buffer = malloc(audio_size);
    
    if (audio_buffer) {
        // 填充模拟音频数据（正弦波）
        for (uint32_t i = 0; i < audio_samples; i++) {
            audio_buffer[i] = (int16_t)(32767.0 * sin(2.0 * M_PI * 440.0 * i / sample_rate));
        }
        
        ModalityData* audio_data = modality_data_create(
            MODALITY_AUDIO, 
            DATA_FORMAT_PCM, 
            audio_buffer, 
            audio_size
        );
        
        if (audio_data) {
            audio_data->shape.dims[0] = 1;  // batch
            audio_data->shape.dims[1] = audio_samples;
            audio_data->shape.ndim = 2;
            audio_data->data_type = TENSOR_INT16;
            audio_data->timestamp = 1234567892;
            audio_data->sequence_id = 3;
            audio_data->source_id = strdup("microphone_input");
            audio_data->metadata = strdup("{\"sample_rate\": 16000, \"channels\": 1, \"format\": \"pcm_s16le\"}");
            
            multimodal_data_add(input_data, audio_data);
            LOG_INFO("Added audio modality data");
        }
        
        free(audio_buffer);
    }
    
    // 7. 创建预处理管道
    PreprocessPipeline preprocess_pipeline = preprocess_pipeline_create();
    if (!preprocess_pipeline) {
        LOG_ERROR("Failed to create preprocessing pipeline");
        ret = -1;
        goto cleanup_multimodal_data;
    }
    
    // 为图像添加预处理操作
    PreprocessParams resize_params = {
        .type = PREPROCESS_RESIZE,
        .params.resize = {
            .width = 224,
            .height = 224,
            .method = INTERPOLATION_LINEAR
        }
    };
    
    PreprocessOp resize_op = preprocess_op_create(PREPROCESS_RESIZE, &resize_params);
    if (resize_op) {
        preprocess_pipeline_add_op(preprocess_pipeline, resize_op);
        LOG_INFO("Added resize operation to preprocessing pipeline");
    }
    
    // 归一化操作
    PreprocessParams normalize_params = {
        .type = PREPROCESS_NORMALIZE,
        .params.normalize = {
            .mean = {0.485f, 0.456f, 0.406f, 0.0f},
            .std = {0.229f, 0.224f, 0.225f, 1.0f},
            .channels = 3
        }
    };
    
    PreprocessOp normalize_op = preprocess_op_create(PREPROCESS_NORMALIZE, &normalize_params);
    if (normalize_op) {
        preprocess_pipeline_add_op(preprocess_pipeline, normalize_op);
        LOG_INFO("Added normalization operation to preprocessing pipeline");
    }
    
    // 8. 模拟推理过程
    LOG_INFO("Starting multimodal inference simulation...");
    
    // 获取各模态数据
    ModalityData* text_modal = multimodal_data_get(input_data, MODALITY_TEXT);
    ModalityData* image_modal = multimodal_data_get(input_data, MODALITY_IMAGE);
    ModalityData* audio_modal = multimodal_data_get(input_data, MODALITY_AUDIO);
    
    if (text_modal) {
        LOG_INFO("Text modality: %zu bytes, source: %s", 
                 text_modal->data_size, text_modal->source_id);
    }
    
    if (image_modal) {
        LOG_INFO("Image modality: %dx%dx%d, source: %s", 
                 image_modal->shape.dims[3], image_modal->shape.dims[2], 
                 image_modal->shape.dims[1], image_modal->source_id);
    }
    
    if (audio_modal) {
        LOG_INFO("Audio modality: %d samples, source: %s", 
                 audio_modal->shape.dims[1], audio_modal->source_id);
    }
    
    // 9. 内存池统计
            memory_pool_stats_t pool_stats;
    if (memory_pool_get_stats(memory_pool, &pool_stats) == 0) {
        LOG_INFO("Memory pool statistics:");
        LOG_INFO("  Total size: %zu bytes", pool_stats.total_size);
        LOG_INFO("  Used size: %zu bytes", pool_stats.used_size);
        LOG_INFO("  Free size: %zu bytes", pool_stats.free_size);
        LOG_INFO("  Peak usage: %zu bytes", pool_stats.peak_usage);
        LOG_INFO("  Active blocks: %u", pool_stats.active_blocks);
        LOG_INFO("  Allocations: %u", pool_stats.alloc_count);
        LOG_INFO("  Deallocations: %u", pool_stats.free_count);
    }
    
    // 10. 验证数据完整性
    if (text_modal && modality_data_validate(text_modal)) {
        LOG_INFO("Text modality data is valid");
    }
    
    if (image_modal && modality_data_validate(image_modal)) {
        LOG_INFO("Image modality data is valid");
    }
    
    if (audio_modal && modality_data_validate(audio_modal)) {
        LOG_INFO("Audio modality data is valid");
    }
    
    LOG_INFO("Multimodal inference simulation completed successfully");
    
    // 清理资源
    preprocess_pipeline_destroy(preprocess_pipeline);
    
cleanup_multimodal_data:
    multimodal_data_destroy(input_data);
    
cleanup_plugin_factory:
    plugin_factory_destroy(plugin_factory);
    
cleanup_instance_manager:
    instance_manager_destroy(instance_manager);
    
cleanup_parser:
    model_parser_destroy(parser);
    
cleanup_memory_pool:
    memory_pool_destroy(memory_pool);
    
    if (ret == 0) {
        LOG_INFO("Example completed successfully");
    } else {
        LOG_ERROR("Example failed with code %d", ret);
    }
    
    return ret;
} 