#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/inference_engine.h"
#include "core/plugin_factory.h"
#include "core/model_manager.h"
#include "core/tensor.h"

/**
 * @brief 插件系统使用示例
 * 
 * 这个示例演示了如何使用 Modyn 的插件系统：
 * 1. 发现并加载推理引擎插件
 * 2. 使用插件创建推理引擎
 * 3. 加载模型并执行推理
 * 4. 管理插件生命周期
 */

void print_usage(const char* program_name) {
    printf("用法: %s [选项]\n", program_name);
    printf("选项:\n");
    printf("  -p, --plugin-path <路径>    添加插件搜索路径\n");
    printf("  -l, --load <插件路径>       直接加载指定插件\n");
    printf("  -m, --model <模型路径>      要加载的模型文件\n");
    printf("  -b, --backend <后端类型>    指定后端类型 (onnx, rknn, openvino 等)\n");
    printf("  -d, --discover             发现并列出所有可用插件\n");
    printf("  -h, --help                 显示此帮助信息\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s --plugin-path ./plugins --discover\n", program_name);
    printf("  %s --load ./plugins/libonnx_runtime.so --model model.onnx\n", program_name);
    printf("  %s --backend onnx --model model.onnx\n", program_name);
}

// 插件发现回调函数
void plugin_discovery_callback(const char* plugin_path, const plugin_info_t* info, void* user_data) {
    printf("  发现插件: %s\n", info->name);
    printf("    路径: %s\n", plugin_path);
    printf("    描述: %s\n", info->description);
    printf("    版本: %d.%d.%d\n", info->version.major, info->version.minor, info->version.patch);
    printf("    类型: %d\n", info->type);
    printf("\n");
    
    // 统计发现的插件数量
    int* count = (int*)user_data;
    (*count)++;
}

// 插件加载回调函数
void plugin_load_callback(const char* plugin_name, plugin_status_e status, void* user_data) {
    const char* status_str;
    switch (status) {
        case PLUGIN_STATUS_LOADED:
            status_str = "已加载";
            break;
        case PLUGIN_STATUS_INITIALIZED:
            status_str = "已初始化";
            break;
        case PLUGIN_STATUS_ERROR:
            status_str = "错误";
            break;
        default:
            status_str = "未知";
            break;
    }
    
    printf("插件状态变更: %s -> %s\n", plugin_name, status_str);
}

int discover_plugins(void) {
    printf("=== 发现可用插件 ===\n");
    
    // 发现系统级插件
    int discovered_count = infer_engine_discover_plugins();
    if (discovered_count < 0) {
        printf("❌ 插件发现失败\n");
        return -1;
    }
    
    printf("✅ 发现 %d 个插件\n\n", discovered_count);
    
    // 获取可用后端
    InferBackendType backends[16];
    uint32_t backend_count = 16;
    
    if (infer_engine_get_available_backends(backends, &backend_count) == 0) {
        printf("=== 可用推理后端 ===\n");
        for (uint32_t i = 0; i < backend_count; i++) {
            const char* backend_name = infer_engine_get_backend_name(backends[i]);
            printf("  %d: %s\n", backends[i], backend_name);
        }
        printf("\n");
    }
    
    return discovered_count;
}

int test_plugin_inference(const char* model_path, InferBackendType backend) {
    printf("=== 测试插件推理 ===\n");
    printf("模型路径: %s\n", model_path ? model_path : "无");
    printf("后端类型: %s\n", infer_engine_get_backend_name(backend));
    
    // 创建推理引擎配置
    InferEngineConfig config = {
        .backend = backend,
        .device_id = 0,
        .num_threads = 4,
        .enable_fp16 = false,
        .enable_int8 = false,
        .custom_config = NULL
    };
    
    // 创建推理引擎（会自动从插件加载）
    printf("创建推理引擎...\n");
    InferEngine engine = infer_engine_create(backend, &config);
    if (!engine) {
        printf("❌ 创建推理引擎失败\n");
        return -1;
    }
    printf("✅ 推理引擎创建成功\n");
    
    // 如果提供了模型路径，尝试加载模型
    if (model_path) {
        printf("加载模型: %s\n", model_path);
        if (infer_engine_load_model(engine, model_path, NULL, 0) != 0) {
            printf("❌ 模型加载失败\n");
            infer_engine_destroy(engine);
            return -1;
        }
        printf("✅ 模型加载成功\n");
        
        // 创建测试张量
        Tensor input_tensor = {0};
        input_tensor.shape.ndim = 4;
        input_tensor.shape.dims[0] = 1;   // batch
        input_tensor.shape.dims[1] = 3;   // channel
        input_tensor.shape.dims[2] = 224; // height
        input_tensor.shape.dims[3] = 224; // width
        input_tensor.dtype = TENSOR_DTYPE_FLOAT32;
        input_tensor.format = TENSOR_FORMAT_NCHW;
        input_tensor.size = 1 * 3 * 224 * 224 * sizeof(float);
        
        // 分配输入数据
        input_tensor.data = calloc(1, input_tensor.size);
        if (!input_tensor.data) {
            printf("❌ 分配输入数据失败\n");
            infer_engine_destroy(engine);
            return -1;
        }
        
        // 填充随机数据
        float* input_data = (float*)input_tensor.data;
        for (uint32_t i = 0; i < input_tensor.size / sizeof(float); i++) {
            input_data[i] = (float)(rand() % 1000) / 1000.0f;
        }
        
        // 创建输出张量
        Tensor output_tensor = {0};
        output_tensor.shape.ndim = 2;
        output_tensor.shape.dims[0] = 1;    // batch
        output_tensor.shape.dims[1] = 1000; // classes
        output_tensor.dtype = TENSOR_DTYPE_FLOAT32;
        output_tensor.format = TENSOR_FORMAT_NCHW;
        output_tensor.size = 1 * 1000 * sizeof(float);
        
        // 分配输出数据
        output_tensor.data = calloc(1, output_tensor.size);
        if (!output_tensor.data) {
            printf("❌ 分配输出数据失败\n");
            free(input_tensor.data);
            infer_engine_destroy(engine);
            return -1;
        }
        
        // 执行推理
        printf("执行推理...\n");
        if (infer_engine_infer(engine, &input_tensor, 1, &output_tensor, 1) != 0) {
            printf("❌ 推理执行失败\n");
            free(input_tensor.data);
            free(output_tensor.data);
            infer_engine_destroy(engine);
            return -1;
        }
        printf("✅ 推理执行成功\n");
        
        // 显示部分输出结果
        float* output_data = (float*)output_tensor.data;
        printf("输出样例 (前10个值):\n");
        for (int i = 0; i < 10 && i < (int)(output_tensor.size / sizeof(float)); i++) {
            printf("  [%d]: %.6f\n", i, output_data[i]);
        }
        
        // 清理
        free(input_tensor.data);
        free(output_tensor.data);
    }
    
    // 销毁推理引擎
    infer_engine_destroy(engine);
    printf("✅ 推理引擎已销毁\n");
    
    return 0;
}

int main(int argc, char* argv[]) {
    printf("=== Modyn 插件系统使用示例 ===\n\n");
    
    // 解析命令行参数
    const char* plugin_path = NULL;
    const char* plugin_file = NULL;
    const char* model_path = NULL;
    const char* backend_str = NULL;
    bool discover = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--plugin-path") == 0) {
            if (i + 1 < argc) {
                plugin_path = argv[++i];
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--load") == 0) {
            if (i + 1 < argc) {
                plugin_file = argv[++i];
            }
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--model") == 0) {
            if (i + 1 < argc) {
                model_path = argv[++i];
            }
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--backend") == 0) {
            if (i + 1 < argc) {
                backend_str = argv[++i];
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--discover") == 0) {
            discover = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // 注册插件搜索路径
    if (plugin_path) {
        printf("添加插件搜索路径: %s\n", plugin_path);
        if (infer_engine_register_plugin_path(plugin_path) != 0) {
            printf("❌ 添加插件搜索路径失败\n");
            return 1;
        }
    }
    
    // 直接加载插件文件
    if (plugin_file) {
        printf("直接加载插件: %s\n", plugin_file);
        if (infer_engine_load_plugin(plugin_file) != 0) {
            printf("❌ 加载插件失败\n");
            return 1;
        }
        printf("✅ 插件加载成功\n\n");
    }
    
    // 发现插件
    if (discover || (!plugin_file && !backend_str)) {
        if (discover_plugins() < 0) {
            return 1;
        }
    }
    
    // 解析后端类型
    InferBackendType backend = INFER_BACKEND_DUMMY; // 默认使用虚拟后端
    if (backend_str) {
        if (strcmp(backend_str, "onnx") == 0) {
            backend = INFER_BACKEND_ONNX;
        } else if (strcmp(backend_str, "rknn") == 0) {
            backend = INFER_BACKEND_RKNN;
        } else if (strcmp(backend_str, "openvino") == 0) {
            backend = INFER_BACKEND_OPENVINO;
        } else if (strcmp(backend_str, "tensorrt") == 0) {
            backend = INFER_BACKEND_TENSORRT;
        } else if (strcmp(backend_str, "dummy") == 0) {
            backend = INFER_BACKEND_DUMMY;
        } else {
            printf("❌ 未知的后端类型: %s\n", backend_str);
            return 1;
        }
    } else if (model_path) {
        // 自动检测后端类型
        backend = infer_engine_detect_backend(model_path);
        printf("自动检测到后端类型: %s\n", infer_engine_get_backend_name(backend));
    }
    
    // 测试推理
    if (test_plugin_inference(model_path, backend) != 0) {
        return 1;
    }
    
    printf("\n=== 示例完成 ===\n");
    return 0;
} 