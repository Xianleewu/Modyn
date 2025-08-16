/**
 * @file model_loader_example.c
 * @brief 模型加载器示例实现，支持解密和解压缩
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "modyn.h"

// ======================= 加密模型加载器 ======================= //

/**
 * @brief AES加密模型加载器私有数据
 */
typedef struct {
    uint8_t aes_key[32];        // AES-256密钥
    uint8_t iv[16];             // 初始化向量
    char key_file_path[256];    // 密钥文件路径
} aes_loader_private_t;

/**
 * @brief AES加密配置
 */
typedef struct {
    const char *key_file;       // 密钥文件路径
    const char *password;       // 密码 (可选)
    int use_hardware_crypto;    // 使用硬件加密
} aes_decrypt_config_t;

/**
 * @brief 检查数据源是否为AES加密模型
 */
static modyn_status_e aes_can_load(const modyn_model_data_source_t *source, modyn_model_format_e *detected_format) {
    uint8_t header[16];
    size_t read = 0;
    
    switch (source->type) {
        case MODYN_MODEL_SOURCE_FILE: {
            FILE *file = fopen(source->source.file.path, "rb");
            if (!file) return MODYN_ERROR_INVALID_ARGUMENT;
            read = fread(header, 1, 16, file);
            fclose(file);
            break;
        }
        
        case MODYN_MODEL_SOURCE_BUFFER: {
            if (source->source.buffer.size < 16) return MODYN_ERROR_INVALID_ARGUMENT;
            memcpy(header, source->source.buffer.data, 16);
            read = 16;
            break;
        }
        
        case MODYN_MODEL_SOURCE_EMBEDDED: {
            if (source->source.embedded.size < 16) return MODYN_ERROR_INVALID_ARGUMENT;
            memcpy(header, source->source.embedded.data, 16);
            read = 16;
            break;
        }
        
        default:
            return MODYN_ERROR_INVALID_ARGUMENT;
    }
    
    if (read != 16) return MODYN_ERROR_INVALID_ARGUMENT;
    
    // 检查AES加密魔数 "AES_MODEL_"
    if (memcmp(header, "AES_MODEL_", 10) == 0) {
        *detected_format = MODYN_MODEL_FORMAT_ENCRYPTED;
        return MODYN_SUCCESS;
    }
    
    return MODYN_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief 简单的AES解密实现 (示例用)
 */
static modyn_status_e simple_aes_decrypt(const uint8_t *encrypted_data, size_t data_size,
                                     const uint8_t *key, const uint8_t *iv,
                                     uint8_t **decrypted_data, size_t *decrypted_size) {
    // 这里应该调用真实的AES解密库，如 OpenSSL
    // 为了示例，我们简单地复制数据
    (void)key; (void)iv; // 未使用参数
    
    *decrypted_data = malloc(data_size);
    if (!*decrypted_data) return MODYN_ERROR_MEMORY_ALLOCATION;
    
    memcpy(*decrypted_data, encrypted_data, data_size);
    *decrypted_size = data_size;
    
    return MODYN_SUCCESS;
}

/**
 * @brief AES加密模型从数据源加载
 */
static modyn_status_e aes_load_model(const modyn_model_data_source_t *source,
                                 const void *config,
                                 model_data_buffer_t *output_buffer,
                                 model_load_info_t *load_info) {
    const aes_decrypt_config_t *decrypt_config = (const aes_decrypt_config_t*)config;
    
    uint8_t *encrypted_data = NULL;
    size_t data_size = 0;
    
    // 根据数据源类型读取数据
    switch (source->type) {
        case MODYN_MODEL_SOURCE_FILE: {
            FILE *file = fopen(source->source.file.path, "rb");
            if (!file) return MODYN_ERROR_MODEL_LOAD_FAILED;
            
            fseek(file, 0, SEEK_END);
            data_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            encrypted_data = malloc(data_size);
            if (!encrypted_data) {
                fclose(file);
                return MODYN_ERROR_MEMORY_ALLOCATION;
            }
            
            fread(encrypted_data, 1, data_size, file);
            fclose(file);
            break;
        }
        
        case MODYN_MODEL_SOURCE_BUFFER: {
            data_size = source->source.buffer.size;
            encrypted_data = malloc(data_size);
            if (!encrypted_data) return MODYN_ERROR_MEMORY_ALLOCATION;
            
            memcpy(encrypted_data, source->source.buffer.data, data_size);
            break;
        }
        
        case MODYN_MODEL_SOURCE_EMBEDDED: {
            data_size = source->source.embedded.size;
            encrypted_data = malloc(data_size);
            if (!encrypted_data) return MODYN_ERROR_MEMORY_ALLOCATION;
            
            memcpy(encrypted_data, source->source.embedded.data, data_size);
            break;
        }
        
        default:
            return MODYN_ERROR_INVALID_ARGUMENT;
    }
    
    // 跳过文件头，获取实际加密数据
    uint8_t *actual_data = encrypted_data + 16; // 跳过16字节头
    size_t actual_size = data_size - 16;
    
    // 加载密钥 (简化实现)
    uint8_t aes_key[32] = {0}; // 实际应该从密钥文件加载
    uint8_t iv[16] = {0};
    
    if (decrypt_config && decrypt_config->key_file) {
        // 从密钥文件加载密钥
        FILE *key_file = fopen(decrypt_config->key_file, "rb");
        if (key_file) {
            fread(aes_key, 1, 32, key_file);
            fread(iv, 1, 16, key_file);
            fclose(key_file);
        }
    }
    
    // 解密模型数据
    uint8_t *decrypted_data;
    size_t decrypted_size;
    modyn_status_e status = simple_aes_decrypt(actual_data, actual_size,
                                           aes_key, iv,
                                           &decrypted_data, &decrypted_size);
    
    free(encrypted_data);
    
    if (status != MODYN_SUCCESS) {
        return MODEL_LOAD_DECRYPT_FAILED;
    }
    
    // 设置输出缓冲区
    output_buffer->data = decrypted_data;
    output_buffer->size = decrypted_size;
    output_buffer->memory_type = MEMORY_INTERNAL;
    output_buffer->owns_memory = 1;
    
    // 设置加载信息
    if (load_info) {
        load_info->source = *source;  // 复制数据源信息
        load_info->format = MODYN_MODEL_FORMAT_ENCRYPTED;
        load_info->original_size = data_size;
        load_info->processed_size = decrypted_size;
        strcpy(load_info->checksum, "decrypted_model_hash");
    }
    
    return MODYN_SUCCESS;
}

/**
 * @brief 释放AES解密的模型数据
 */
static modyn_status_e aes_free_model_data(model_data_buffer_t *buffer) {
    if (buffer && buffer->data && buffer->owns_memory) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->size = 0;
    }
    return MODYN_SUCCESS;
}

/**
 * @brief 获取AES加载器信息
 */
static modyn_status_e aes_get_loader_info(char *name, size_t name_size,
                                      char *version, size_t version_size) {
    if (name) strncpy(name, "AES Decryption Loader", name_size - 1);
    if (version) strncpy(version, "1.0.0", version_size - 1);
    return MODYN_SUCCESS;
}

/**
 * @brief 验证AES解密模型的完整性
 */
static modyn_status_e aes_validate_model(const model_data_buffer_t *buffer,
                                     const char *expected_checksum) {
    if (!buffer || !buffer->data) return MODYN_ERROR_INVALID_ARGUMENT;
    
    // 计算SHA256校验和 (简化实现)
    char calculated_checksum[65] = "decrypted_model_hash";
    
    if (expected_checksum && strcmp(calculated_checksum, expected_checksum) != 0) {
        return MODYN_ERROR_INVALID_ARGUMENT;
    }
    
    return MODYN_SUCCESS;
}

// AES加密模型加载器操作函数表
static model_loader_ops_t aes_loader_ops = {
    .can_load = aes_can_load,
    .load_model = aes_load_model,
    .free_model_data = aes_free_model_data,
    .get_loader_info = aes_get_loader_info,
    .validate_model = aes_validate_model,
    .extensions = NULL
};

// ======================= 压缩模型加载器 ======================= //

/**
 * @brief 检查是否为压缩模型
 */
static modyn_status_e gzip_can_load(const char *model_path, modyn_model_format_e *detected_format) {
    FILE *file = fopen(model_path, "rb");
    if (!file) return MODYN_ERROR_INVALID_ARGUMENT;
    
    // 检查GZIP魔数
    uint8_t header[3];
    size_t read = fread(header, 1, 3, file);
    fclose(file);
    
    if (read == 3 && header[0] == 0x1f && header[1] == 0x8b && header[2] == 0x08) {
        *detected_format = MODEL_FORMAT_COMPRESSED;
        return MODYN_SUCCESS;
    }
    
    return MODYN_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief GZIP压缩模型加载
 */
static modyn_status_e gzip_load_model(const char *model_path,
                                  const void *config,
                                  model_data_buffer_t *output_buffer,
                                  model_load_info_t *load_info) {
    (void)config; // 未使用参数
    
    // 这里应该调用zlib进行解压缩
    // 为了示例，我们简单地读取文件
    FILE *file = fopen(model_path, "rb");
    if (!file) return MODYN_ERROR_MODEL_LOAD_FAILED;
    
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    uint8_t *compressed_data = malloc(file_size);
    if (!compressed_data) {
        fclose(file);
        return MODYN_ERROR_MEMORY_ALLOCATION;
    }
    
    fread(compressed_data, 1, file_size, file);
    fclose(file);
    
    // 模拟解压缩 (实际应该调用zlib)
    size_t decompressed_size = file_size * 3; // 假设解压后是3倍大小
    uint8_t *decompressed_data = malloc(decompressed_size);
    if (!decompressed_data) {
        free(compressed_data);
        return MODYN_ERROR_MEMORY_ALLOCATION;
    }
    
    // 这里应该是真实的解压缩代码
    memcpy(decompressed_data, compressed_data, file_size);
    
    free(compressed_data);
    
    // 设置输出缓冲区
    output_buffer->data = decompressed_data;
    output_buffer->size = decompressed_size;
    output_buffer->memory_type = MEMORY_INTERNAL;
    output_buffer->owns_memory = 1;
    
    // 设置加载信息
    if (load_info) {
        strncpy(load_info->original_path, model_path, sizeof(load_info->original_path) - 1);
        load_info->format = MODEL_FORMAT_COMPRESSED;
        load_info->original_size = file_size;
        load_info->processed_size = decompressed_size;
        strcpy(load_info->checksum, "decompressed_model_hash");
    }
    
    return MODYN_SUCCESS;
}

// GZIP压缩模型加载器操作函数表
static model_loader_ops_t gzip_loader_ops = {
    .can_load = gzip_can_load,
    .load_model = gzip_load_model,
    .free_model_data = aes_free_model_data, // 复用相同的释放函数
    .get_loader_info = NULL,
    .validate_model = NULL,
    .extensions = NULL
};

// ======================= 加载器注册 ======================= //

/**
 * @brief AES加密模型加载器
 */
static model_loader_t aes_model_loader = {
    .name = "AES_Decryption_Loader",
    .supported_formats = {MODYN_MODEL_FORMAT_ENCRYPTED, MODYN_MODEL_FORMAT_ENCRYPTED_COMPRESSED},
    .priority = 100, // 高优先级
    .ops = &aes_loader_ops,
    .private_data = NULL
};

/**
 * @brief GZIP压缩模型加载器
 */
static model_loader_t gzip_model_loader = {
    .name = "GZIP_Decompression_Loader",
    .supported_formats = {MODEL_FORMAT_COMPRESSED},
    .priority = 50, // 中等优先级
    .ops = &gzip_loader_ops,
    .private_data = NULL
};

/**
 * @brief 注册所有示例加载器
 */
modyn_status_e register_example_model_loaders(void) {
    modyn_status_e status;
    
    // 注册AES解密加载器
    status = modyn_register_model_loader(&aes_model_loader);
    if (status != MODYN_SUCCESS) {
        printf("Failed to register AES loader: %d\n", status);
        return status;
    }
    
    // 注册GZIP解压缩加载器
    status = modyn_register_model_loader(&gzip_model_loader);
    if (status != MODYN_SUCCESS) {
        printf("Failed to register GZIP loader: %d\n", status);
        return status;
    }
    
    printf("Model loaders registered successfully\n");
    return MODYN_SUCCESS;
}

/**
 * @brief 使用示例：从文件加载加密模型
 */
void example_load_encrypted_model_from_file(void) {
    // 1. 注册模型加载器
    register_example_model_loaders();
    
    // 2. 配置AES解密参数
    aes_decrypt_config_t decrypt_config = {
        .key_file = "/path/to/aes.key",
        .password = "my_secret_password",
        .use_hardware_crypto = 1
    };
    
    // 3. 从文件加载加密模型
    model_data_buffer_t buffer = {0};
    model_load_info_t load_info = {0};
    
    modyn_status_e status = modyn_load_model_with_loader("encrypted_model.enc",
                                                  &decrypt_config,
                                                  &buffer,
                                                  &load_info);
    
    if (status == MODYN_SUCCESS) {
        printf("成功从文件加载加密模型:\n");
        printf("  原始大小: %zu bytes\n", load_info.original_size);
        printf("  解密后大小: %zu bytes\n", load_info.processed_size);
        printf("  格式: %d\n", load_info.format);
        printf("  校验和: %s\n", load_info.checksum);
        
        // 释放模型数据
        modyn_free_model_buffer(&buffer);
    } else {
        printf("加载加密模型失败: %d\n", status);
    }
}

/**
 * @brief 使用示例：从内存缓冲区加载加密模型
 */
void example_load_encrypted_model_from_buffer(void) {
    // 1. 模拟从网络或其他来源获取的加密模型数据
    uint8_t encrypted_model_data[] = {
        'A', 'E', 'S', '_', 'M', 'O', 'D', 'E', 'L', '_', 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        // 模拟加密的模型数据...
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        // 更多加密数据...
    };
    size_t data_size = sizeof(encrypted_model_data);
    
    // 2. 配置解密参数
    aes_decrypt_config_t decrypt_config = {
        .key_file = "/path/to/aes.key",
        .password = "buffer_secret",
        .use_hardware_crypto = 0
    };
    
    // 3. 从缓冲区加载模型
    model_data_buffer_t buffer = {0};
    model_load_info_t load_info = {0};
    
    modyn_status_e status = modyn_load_model_from_buffer(encrypted_model_data, data_size,
                                                  &decrypt_config,
                                                  &buffer, &load_info);
    
    if (status == MODYN_SUCCESS) {
        printf("成功从缓冲区加载加密模型:\n");
        printf("  数据源类型: %d (BUFFER)\n", load_info.source.type);
        printf("  原始大小: %zu bytes\n", load_info.original_size);
        printf("  解密后大小: %zu bytes\n", load_info.processed_size);
        
        modyn_free_model_buffer(&buffer);
    } else {
        printf("从缓冲区加载模型失败: %d\n", status);
    }
}

/**
 * @brief 使用示例：从嵌入式资源加载模型
 */
void example_load_model_from_embedded_resource(void) {
    // 1. 模拟嵌入式资源数据 (通常通过objcopy等工具嵌入)
    extern const uint8_t embedded_model_start[];
    extern const uint8_t embedded_model_end[];
    size_t embedded_size = embedded_model_end - embedded_model_start;
    
    // 2. 创建嵌入式资源数据源
    modyn_model_data_source_t source;
    modyn_create_embedded_source("ai_model_v1.0", embedded_model_start, 
                             embedded_size, &source);
    
    // 3. 从嵌入式资源加载模型
    model_data_buffer_t buffer = {0};
    model_load_info_t load_info = {0};
    
    modyn_status_e status = modyn_load_model_from_source(&source, NULL, &buffer, &load_info);
    
    if (status == MODYN_SUCCESS) {
        printf("成功从嵌入式资源加载模型:\n");
        printf("  资源ID: %s\n", load_info.source.source.embedded.resource_id);
        printf("  模型大小: %zu bytes\n", load_info.processed_size);
        
        modyn_free_model_buffer(&buffer);
    }
    
    // 4. 清理数据源
    modyn_destroy_data_source(&source);
}

/**
 * @brief 使用示例：从URL加载模型
 */
void example_load_model_from_url(void) {
    // 1. 从网络URL加载模型
    const char *model_url = "https://models.example.com/ai_model.onnx";
    const char *headers = "Authorization: Bearer your_token\r\n"
                         "User-Agent: AIFramework/1.0\r\n";
    
    model_data_buffer_t buffer = {0};
    model_load_info_t load_info = {0};
    
    modyn_status_e status = modyn_load_model_from_url(model_url, headers, 30, // 30秒超时
                                               NULL, &buffer, &load_info);
    
    if (status == MODYN_SUCCESS) {
        printf("成功从URL加载模型:\n");
        printf("  URL: %s\n", load_info.source.source.url.url);
        printf("  下载大小: %zu bytes\n", load_info.processed_size);
        
        modyn_free_model_buffer(&buffer);
    } else {
        printf("从URL加载模型失败: %d\n", status);
    }
}

/**
 * @brief 综合示例：多种数据源的统一处理
 */
void example_unified_model_loading(void) {
    register_example_model_loaders();
    
    // 定义多种数据源
    modyn_model_data_source_t sources[4];
    
    // 1. 文件数据源
    modyn_create_file_source("model.enc", &sources[0]);
    
    // 2. 缓冲区数据源
    uint8_t model_buffer[1024] = {0}; // 模拟数据
    modyn_create_buffer_source(model_buffer, sizeof(model_buffer), 0, &sources[1]);
    
    // 3. URL数据源
    modyn_create_url_source("https://cdn.example.com/model.gz", NULL, 60, &sources[2]);
    
    // 4. 嵌入式资源数据源
    uint8_t embedded_data[] = {0x50, 0x4B, 0x03, 0x04}; // ZIP header
    modyn_create_embedded_source("builtin_model", embedded_data, sizeof(embedded_data), &sources[3]);
    
    // 统一加载处理
    for (int i = 0; i < 4; i++) {
        model_data_buffer_t buffer = {0};
        model_load_info_t load_info = {0};
        
        modyn_status_e status = modyn_load_model_from_source(&sources[i], NULL, &buffer, &load_info);
        
        if (status == MODYN_SUCCESS) {
            printf("数据源 %d 加载成功，类型: %d，大小: %zu bytes\n", 
                   i, load_info.source.type, load_info.processed_size);
            modyn_free_model_buffer(&buffer);
        } else {
            printf("数据源 %d 加载失败: %d\n", i, status);
        }
        
        // 清理数据源
        modyn_destroy_data_source(&sources[i]);
    }
}