#include "core/multimodal.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 获取当前时间戳
static uint64_t get_current_timestamp(void) {
    return (uint64_t)time(NULL);
}

ModalityData* modality_data_create(ModalityType modality, DataFormat format, 
                                  const void* data, size_t data_size) {
    if (!data || data_size == 0) return NULL;
    
    ModalityData* modal_data = calloc(1, sizeof(ModalityData));
    if (!modal_data) {
        LOG_ERROR("Failed to allocate modality data");
        return NULL;
    }
    
    modal_data->modality = modality;
    modal_data->format = format;
    modal_data->data_size = data_size;
    modal_data->timestamp = get_current_timestamp();
    
    // 复制数据
    modal_data->data = malloc(data_size);
    if (!modal_data->data) {
        LOG_ERROR("Failed to allocate data buffer");
        free(modal_data);
        return NULL;
    }
    
    memcpy(modal_data->data, data, data_size);
    
    // 设置默认数据类型
    switch (modality) {
        case MODALITY_TEXT:
            modal_data->data_type = TENSOR_TYPE_UINT8;
            break;
        case MODALITY_IMAGE:
            modal_data->data_type = TENSOR_TYPE_UINT8;
            break;
        case MODALITY_AUDIO:
            modal_data->data_type = TENSOR_TYPE_INT16;
            break;
        default:
            modal_data->data_type = TENSOR_TYPE_FLOAT32;
            break;
    }
    
    LOG_DEBUG("Created modality data: type=%s, format=%s, size=%zu", 
              modality_type_to_string(modality), 
              data_format_to_string(format), 
              data_size);
    
    return modal_data;
}

void modality_data_destroy(ModalityData* modal_data) {
    if (!modal_data) return;
    
    free(modal_data->data);
    free(modal_data->metadata);
    free(modal_data->source_id);
    free(modal_data);
}

ModalityData* modality_data_copy(const ModalityData* src) {
    if (!src) return NULL;
    
    ModalityData* copy = modality_data_create(src->modality, src->format, 
                                             src->data, src->data_size);
    if (!copy) return NULL;
    
    copy->shape = src->shape;
    copy->data_type = src->data_type;
    copy->timestamp = src->timestamp;
    copy->sequence_id = src->sequence_id;
    
    if (src->metadata) {
        copy->metadata = strdup(src->metadata);
    }
    
    if (src->source_id) {
        copy->source_id = strdup(src->source_id);
    }
    
    return copy;
}

MultiModalData* multimodal_data_create(uint32_t capacity) {
    if (capacity == 0) capacity = 4;  // 默认容量
    
    MultiModalData* multi_data = calloc(1, sizeof(MultiModalData));
    if (!multi_data) {
        LOG_ERROR("Failed to allocate multimodal data");
        return NULL;
    }
    
    multi_data->modalities = calloc(capacity, sizeof(ModalityData));
    if (!multi_data->modalities) {
        LOG_ERROR("Failed to allocate modalities array");
        free(multi_data);
        return NULL;
    }
    
    multi_data->capacity = capacity;
    multi_data->modality_count = 0;
    multi_data->created_time = get_current_timestamp();
    
    LOG_DEBUG("Created multimodal data container with capacity %u", capacity);
    
    return multi_data;
}

void multimodal_data_destroy(MultiModalData* multi_data) {
    if (!multi_data) return;
    
    for (uint32_t i = 0; i < multi_data->modality_count; i++) {
        ModalityData* modal = &multi_data->modalities[i];
        free(modal->data);
        free(modal->metadata);
        free(modal->source_id);
    }
    
    free(multi_data->modalities);
    free(multi_data->session_id);
    free(multi_data);
}

int multimodal_data_add(MultiModalData* multi_data, const ModalityData* modal_data) {
    if (!multi_data || !modal_data) return -1;
    
    // 检查是否需要扩容
    if (multi_data->modality_count >= multi_data->capacity) {
        uint32_t new_capacity = multi_data->capacity * 2;
        ModalityData* new_modalities = realloc(multi_data->modalities, 
                                              new_capacity * sizeof(ModalityData));
        if (!new_modalities) {
            LOG_ERROR("Failed to expand modalities array");
            return -1;
        }
        
        multi_data->modalities = new_modalities;
        multi_data->capacity = new_capacity;
        
        // 初始化新分配的内存
        memset(&multi_data->modalities[multi_data->modality_count], 0, 
               (new_capacity - multi_data->modality_count) * sizeof(ModalityData));
    }
    
    // 复制模态数据
    ModalityData* dst = &multi_data->modalities[multi_data->modality_count];
    *dst = *modal_data;
    
    // 深拷贝字符串字段
    if (modal_data->metadata) {
        dst->metadata = strdup(modal_data->metadata);
    }
    
    if (modal_data->source_id) {
        dst->source_id = strdup(modal_data->source_id);
    }
    
    // 深拷贝数据
    if (modal_data->data && modal_data->data_size > 0) {
        dst->data = malloc(modal_data->data_size);
        if (!dst->data) {
            LOG_ERROR("Failed to copy modality data");
            return -1;
        }
        memcpy(dst->data, modal_data->data, modal_data->data_size);
    }
    
    multi_data->modality_count++;
    
    LOG_DEBUG("Added modality data: type=%s, count=%u", 
              modality_type_to_string(modal_data->modality), 
              multi_data->modality_count);
    
    return 0;
}

ModalityData* multimodal_data_get(const MultiModalData* multi_data, ModalityType modality) {
    if (!multi_data) return NULL;
    
    for (uint32_t i = 0; i < multi_data->modality_count; i++) {
        if (multi_data->modalities[i].modality == modality) {
            return &multi_data->modalities[i];
        }
    }
    
    return NULL;
}

int multimodal_data_remove(MultiModalData* multi_data, ModalityType modality) {
    if (!multi_data) return -1;
    
    for (uint32_t i = 0; i < multi_data->modality_count; i++) {
        if (multi_data->modalities[i].modality == modality) {
            ModalityData* modal = &multi_data->modalities[i];
            
            // 释放内存
            free(modal->data);
            free(modal->metadata);
            free(modal->source_id);
            
            // 移动后续元素
            if (i < multi_data->modality_count - 1) {
                memmove(&multi_data->modalities[i], 
                       &multi_data->modalities[i + 1],
                       (multi_data->modality_count - i - 1) * sizeof(ModalityData));
            }
            
            multi_data->modality_count--;
            
            LOG_DEBUG("Removed modality data: type=%s", modality_type_to_string(modality));
            return 0;
        }
    }
    
    return -1;  // 未找到
}

const char* modality_type_to_string(ModalityType modality) {
    switch (modality) {
        case MODALITY_TEXT:        return "Text";
        case MODALITY_IMAGE:       return "Image";
        case MODALITY_AUDIO:       return "Audio";
        case MODALITY_VIDEO:       return "Video";
        case MODALITY_POINT_CLOUD: return "PointCloud";
        case MODALITY_DEPTH:       return "Depth";
        case MODALITY_THERMAL:     return "Thermal";
        case MODALITY_RADAR:       return "Radar";
        case MODALITY_LIDAR:       return "LiDAR";
        case MODALITY_SENSOR:      return "Sensor";
        case MODALITY_CUSTOM:      return "Custom";
        default:                   return "Unknown";
    }
}

ModalityType modality_type_from_string(const char* modality_str) {
    if (!modality_str) return MODALITY_UNKNOWN;
    
    if (strcasecmp(modality_str, "Text") == 0) return MODALITY_TEXT;
    if (strcasecmp(modality_str, "Image") == 0) return MODALITY_IMAGE;
    if (strcasecmp(modality_str, "Audio") == 0) return MODALITY_AUDIO;
    if (strcasecmp(modality_str, "Video") == 0) return MODALITY_VIDEO;
    if (strcasecmp(modality_str, "PointCloud") == 0) return MODALITY_POINT_CLOUD;
    if (strcasecmp(modality_str, "Depth") == 0) return MODALITY_DEPTH;
    if (strcasecmp(modality_str, "Thermal") == 0) return MODALITY_THERMAL;
    if (strcasecmp(modality_str, "Radar") == 0) return MODALITY_RADAR;
    if (strcasecmp(modality_str, "LiDAR") == 0) return MODALITY_LIDAR;
    if (strcasecmp(modality_str, "Sensor") == 0) return MODALITY_SENSOR;
    if (strcasecmp(modality_str, "Custom") == 0) return MODALITY_CUSTOM;
    
    return MODALITY_UNKNOWN;
}

const char* data_format_to_string(DataFormat format) {
    switch (format) {
        // 文本格式
        case DATA_FORMAT_UTF8:         return "UTF-8";
        case DATA_FORMAT_ASCII:        return "ASCII";
        case DATA_FORMAT_TOKEN:        return "Token";
        case DATA_FORMAT_EMBEDDING:    return "Embedding";
        
        // 图像格式
        case DATA_FORMAT_RGB:          return "RGB";
        case DATA_FORMAT_BGR:          return "BGR";
        case DATA_FORMAT_RGBA:         return "RGBA";
        case DATA_FORMAT_GRAY:         return "Grayscale";
        case DATA_FORMAT_YUV:          return "YUV";
        case DATA_FORMAT_HSV:          return "HSV";
        case DATA_FORMAT_JPEG:         return "JPEG";
        case DATA_FORMAT_PNG:          return "PNG";
        
        // 音频格式
        case DATA_FORMAT_PCM:          return "PCM";
        case DATA_FORMAT_WAV:          return "WAV";
        case DATA_FORMAT_MP3:          return "MP3";
        case DATA_FORMAT_AAC:          return "AAC";
        case DATA_FORMAT_FLAC:         return "FLAC";
        case DATA_FORMAT_SPECTROGRAM:  return "Spectrogram";
        case DATA_FORMAT_MFCC:         return "MFCC";
        
        // 视频格式
        case DATA_FORMAT_H264:         return "H.264";
        case DATA_FORMAT_H265:         return "H.265";
        case DATA_FORMAT_VP8:          return "VP8";
        case DATA_FORMAT_VP9:          return "VP9";
        case DATA_FORMAT_AV1:          return "AV1";
        
        // 3D格式
        case DATA_FORMAT_PLY:          return "PLY";
        case DATA_FORMAT_PCD:          return "PCD";
        case DATA_FORMAT_OBJ:          return "OBJ";
        case DATA_FORMAT_STL:          return "STL";
        
        case DATA_FORMAT_CUSTOM:       return "Custom";
        default:                       return "Unknown";
    }
}

DataFormat data_format_from_string(const char* format_str) {
    if (!format_str) return DATA_FORMAT_UNKNOWN;
    
    // 文本格式
    if (strcasecmp(format_str, "UTF-8") == 0) return DATA_FORMAT_UTF8;
    if (strcasecmp(format_str, "ASCII") == 0) return DATA_FORMAT_ASCII;
    if (strcasecmp(format_str, "Token") == 0) return DATA_FORMAT_TOKEN;
    if (strcasecmp(format_str, "Embedding") == 0) return DATA_FORMAT_EMBEDDING;
    
    // 图像格式
    if (strcasecmp(format_str, "RGB") == 0) return DATA_FORMAT_RGB;
    if (strcasecmp(format_str, "BGR") == 0) return DATA_FORMAT_BGR;
    if (strcasecmp(format_str, "RGBA") == 0) return DATA_FORMAT_RGBA;
    if (strcasecmp(format_str, "Grayscale") == 0) return DATA_FORMAT_GRAY;
    if (strcasecmp(format_str, "YUV") == 0) return DATA_FORMAT_YUV;
    if (strcasecmp(format_str, "HSV") == 0) return DATA_FORMAT_HSV;
    if (strcasecmp(format_str, "JPEG") == 0) return DATA_FORMAT_JPEG;
    if (strcasecmp(format_str, "PNG") == 0) return DATA_FORMAT_PNG;
    
    // 音频格式
    if (strcasecmp(format_str, "PCM") == 0) return DATA_FORMAT_PCM;
    if (strcasecmp(format_str, "WAV") == 0) return DATA_FORMAT_WAV;
    if (strcasecmp(format_str, "MP3") == 0) return DATA_FORMAT_MP3;
    if (strcasecmp(format_str, "AAC") == 0) return DATA_FORMAT_AAC;
    if (strcasecmp(format_str, "FLAC") == 0) return DATA_FORMAT_FLAC;
    if (strcasecmp(format_str, "Spectrogram") == 0) return DATA_FORMAT_SPECTROGRAM;
    if (strcasecmp(format_str, "MFCC") == 0) return DATA_FORMAT_MFCC;
    
    // 其他格式类似...
    
    return DATA_FORMAT_UNKNOWN;
}

bool modality_is_compatible(ModalityType modality1, ModalityType modality2) {
    // 相同模态总是兼容的
    if (modality1 == modality2) return true;
    
    // 定义一些兼容性规则
    switch (modality1) {
        case MODALITY_IMAGE:
            return modality2 == MODALITY_VIDEO || modality2 == MODALITY_DEPTH || 
                   modality2 == MODALITY_THERMAL;
            
        case MODALITY_VIDEO:
            return modality2 == MODALITY_IMAGE || modality2 == MODALITY_AUDIO;
            
        case MODALITY_POINT_CLOUD:
            return modality2 == MODALITY_DEPTH || modality2 == MODALITY_LIDAR;
            
        case MODALITY_SENSOR:
            return true;  // 传感器数据与所有模态兼容
            
        default:
            return false;
    }
}

size_t modality_data_get_size(const ModalityData* modal_data) {
    if (!modal_data) return 0;
    return modal_data->data_size;
}

bool modality_data_validate(const ModalityData* modal_data) {
    if (!modal_data) return false;
    
    // 检查基本字段
    if (modal_data->modality == MODALITY_UNKNOWN) return false;
    if (modal_data->format == DATA_FORMAT_UNKNOWN) return false;
    if (!modal_data->data || modal_data->data_size == 0) return false;
    
    // 检查形状的有效性
    if (modal_data->shape.ndim > TENSOR_MAX_DIMS) return false;
    
    // 验证数据大小与形状的一致性
    size_t expected_size = 1;
    for (uint32_t i = 0; i < modal_data->shape.ndim; i++) {
        if (modal_data->shape.dims[i] == 0) return false;
        expected_size *= modal_data->shape.dims[i];
    }
    
    // 根据数据类型计算字节大小
    size_t element_size = 1;
    switch (modal_data->data_type) {
        case TENSOR_TYPE_FLOAT32:
        case TENSOR_TYPE_INT32:
            element_size = 4;
            break;
        case TENSOR_TYPE_FLOAT64:
        case TENSOR_TYPE_INT64:
            element_size = 8;
            break;
        case TENSOR_TYPE_FLOAT16:
        case TENSOR_TYPE_INT16:
            element_size = 2;
            break;
        case TENSOR_TYPE_UINT8:
        case TENSOR_TYPE_INT8:
            element_size = 1;
            break;
        case TENSOR_TYPE_BOOL:
            element_size = 1;
            break;
        case TENSOR_TYPE_STRING:
            // 字符串类型大小不固定
            element_size = 1;
            break;
        case TENSOR_TYPE_UNKNOWN:
        default:
            element_size = 1;
            break;
    }
    
    // 对于有形状信息的数据，验证大小
    if (modal_data->shape.ndim > 0 && expected_size * element_size != modal_data->data_size) {
        return false;
    }
    
    return true;
} 