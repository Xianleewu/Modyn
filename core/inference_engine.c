#include "core/inference_engine.h"
#include "core/plugin_factory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 全局工厂注册表
static infer_engine_factory_t* registered_factories[16];
static int factory_count = 0;

// 全局引擎映射表，用于存储引擎和其工厂的对应关系
typedef struct {
    infer_engine_t engine;
    const infer_engine_factory_t* factory;
} EngineMapping;

static EngineMapping engine_mappings[64];
static int mapping_count = 0;

// 全局插件工厂实例
static plugin_factory_t global_plugin_factory = NULL;

// 前向声明
static const infer_engine_factory_t* find_factory(infer_backend_type_e backend);
static const infer_engine_factory_t* find_factory_by_engine(infer_engine_t engine);
static void add_engine_mapping(infer_engine_t engine, const infer_engine_factory_t* factory);
static void remove_engine_mapping(infer_engine_t engine);
static int try_load_backend_from_plugins(infer_backend_type_e backend);
static void initialize_global_plugin_factory(void);

int infer_engine_register_factory(const infer_engine_factory_t* factory) {
    if (!factory || factory_count >= 16) {
        return -1;
    }
    
    // 检查是否已经注册
    for (int i = 0; i < factory_count; i++) {
        if (registered_factories[i]->backend == factory->backend) {
            return -2; // 已存在
        }
    }
    
    // 复制工厂结构
    infer_engine_factory_t* new_factory = malloc(sizeof(infer_engine_factory_t));
    if (!new_factory) {
        return -1;
    }
    
    *new_factory = *factory;
    registered_factories[factory_count++] = new_factory;
    
    printf("注册推理引擎工厂: %s (后端ID: %d)\n", factory->name, factory->backend);
    return 0;
}

static const infer_engine_factory_t* find_factory(infer_backend_type_e backend) {
    for (int i = 0; i < factory_count; i++) {
        if (registered_factories[i]->backend == backend) {
            return registered_factories[i];
        }
    }
    return NULL;
}

infer_engine_t infer_engine_create(infer_backend_type_e backend, const infer_engine_config_t* config) {
    const infer_engine_factory_t* factory = find_factory(backend);
    
    // 如果未找到工厂，尝试从插件中加载
    if (!factory) {
        printf("未找到后端工厂，尝试从插件加载: %d\n", backend);
        if (try_load_backend_from_plugins(backend) == 0) {
            factory = find_factory(backend);
        }
    }
    
    if (!factory) {
        printf("未找到后端工厂: %d\n", backend);
        return NULL;
    }
    
    infer_engine_t engine = factory->ops->create(config);
    if (engine) {
        add_engine_mapping(engine, factory);
    }
    
    return engine;
}

void infer_engine_destroy(infer_engine_t engine) {
    if (!engine) {
        return;
    }
    
    const infer_engine_factory_t* factory = find_factory_by_engine(engine);
    if (factory && factory->ops->destroy) {
        factory->ops->destroy(engine);
        remove_engine_mapping(engine);
    }
}

int infer_engine_get_available_backends(infer_backend_type_e* backends, uint32_t* count) {
    if (!backends || !count) {
        return -1;
    }
    
    uint32_t max_count = *count;
    uint32_t actual_count = 0;
    
    // 添加已注册的后端
    for (int i = 0; i < factory_count && actual_count < max_count; i++) {
        backends[actual_count] = registered_factories[i]->backend;
        actual_count++;
    }
    
    // 初始化插件工厂并发现可用的插件后端
    initialize_global_plugin_factory();
    if (global_plugin_factory) {
        InferBackendType* plugin_backends = NULL;
        uint32_t plugin_count = 0;
        
        if (plugin_factory_get_available_backends(global_plugin_factory, &plugin_backends, &plugin_count) == 0) {
            for (uint32_t i = 0; i < plugin_count && actual_count < max_count; i++) {
                // 检查是否已存在
                bool exists = false;
                for (uint32_t j = 0; j < actual_count; j++) {
                    if (backends[j] == plugin_backends[i]) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    backends[actual_count] = plugin_backends[i];
                    actual_count++;
                }
            }
            
            free(plugin_backends);
        }
    }
    
    *count = actual_count;
    return 0;
}

const char* infer_engine_get_backend_name(infer_backend_type_e backend) {
    const infer_engine_factory_t* factory = find_factory(backend);
    if (factory) {
        return factory->name;
    }
    
    // 提供默认名称
    switch (backend) {
        case INFER_BACKEND_RKNN:
            return "RKNN";
        case INFER_BACKEND_OPENVINO:
            return "OpenVINO";
        case INFER_BACKEND_TENSORRT:
            return "TensorRT";
        case INFER_BACKEND_ONNX:
            return "ONNX Runtime";
        case INFER_BACKEND_DUMMY:
            return "Dummy";
        default:
            return "Unknown";
    }
}

infer_backend_type_e infer_engine_detect_backend(const char* model_path) {
    if (!model_path) {
        return INFER_BACKEND_UNKNOWN;
    }
    
    // 根据文件扩展名检测后端类型
    const char* ext = strrchr(model_path, '.');
    if (!ext) {
        return INFER_BACKEND_DUMMY; // 默认使用虚拟后端
    }
    
    if (strcmp(ext, ".rknn") == 0) {
        return INFER_BACKEND_RKNN;
    } else if (strcmp(ext, ".xml") == 0) {
        return INFER_BACKEND_OPENVINO;
    } else if (strcmp(ext, ".engine") == 0 || strcmp(ext, ".plan") == 0) {
        return INFER_BACKEND_TENSORRT;
    } else if (strcmp(ext, ".onnx") == 0) {
        return INFER_BACKEND_ONNX;
    } else {
        return INFER_BACKEND_DUMMY; // 默认使用虚拟后端
    }
}

// 插件系统集成函数
int infer_engine_load_plugin(const char* plugin_path) {
    if (!plugin_path) return -1;
    
    initialize_global_plugin_factory();
    if (!global_plugin_factory) return -1;
    
    plugin_t plugin = plugin_factory_load_from_file(global_plugin_factory, plugin_path);
    if (!plugin) {
        printf("加载插件失败: %s\n", plugin_path);
        return -1;
    }
    
    const plugin_info_t* info = plugin_get_info(plugin);
    if (info->type == PLUGIN_TYPE_INFERENCE_ENGINE) {
        // 初始化推理引擎插件
        if (plugin_initialize(plugin, NULL) != 0) {
            printf("初始化推理引擎插件失败: %s\n", info->name);
            return -1;
        }
        
        // 获取并注册推理引擎工厂
        const InferEngineFactory* factory = plugin_get_inference_engine_factory(plugin);
        if (factory) {
            int result = infer_engine_register_factory(factory);
            if (result == 0) {
                printf("从插件注册推理引擎成功: %s -> %s\n", info->name, factory->name);
                return 0;
            } else if (result == -2) {
                printf("推理引擎后端已存在: %s\n", factory->name);
                return 0; // 已存在不算错误
            }
        }
    }
    
    printf("插件不是推理引擎类型或注册失败: %s\n", plugin_path);
    return -1;
}

int infer_engine_register_plugin_path(const char* plugin_search_path) {
    if (!plugin_search_path) return -1;
    
    initialize_global_plugin_factory();
    if (!global_plugin_factory) return -1;
    
    return plugin_factory_add_search_path(global_plugin_factory, plugin_search_path);
}

int infer_engine_discover_plugins(void) {
    initialize_global_plugin_factory();
    if (!global_plugin_factory) return -1;
    
    // 添加默认搜索路径
    plugin_factory_add_search_path(global_plugin_factory, "./plugins");
    plugin_factory_add_search_path(global_plugin_factory, "/usr/local/lib/modyn/plugins");
    plugin_factory_add_search_path(global_plugin_factory, "/opt/modyn/plugins");
    
    // 发现插件
    int discovered_count = plugin_factory_discover(global_plugin_factory, NULL, NULL);
    printf("发现 %d 个插件\n", discovered_count);
    
    // 自动加载推理引擎插件
    char** plugin_names = NULL;
    uint32_t plugin_count = 0;
    
    if (plugin_factory_list(global_plugin_factory, &plugin_names, &plugin_count) == 0) {
        for (uint32_t i = 0; i < plugin_count; i++) {
            plugin_t plugin = plugin_factory_get(global_plugin_factory, plugin_names[i]);
            if (plugin) {
                const plugin_info_t* info = plugin_get_info(plugin);
                if (info->type == PLUGIN_TYPE_INFERENCE_ENGINE) {
                    printf("自动加载推理引擎插件: %s\n", info->name);
                    
                    if (plugin_initialize(plugin, NULL) == 0) {
                        const InferEngineFactory* factory = plugin_get_inference_engine_factory(plugin);
                        if (factory) {
                            infer_engine_register_factory(factory);
                        }
                    }
                }
            }
            free(plugin_names[i]);
        }
        free(plugin_names);
    }
    
    return discovered_count;
}

plugin_factory_t infer_engine_get_plugin_factory(void) {
    initialize_global_plugin_factory();
    return global_plugin_factory;
}

// 清理函数，程序退出时调用
static const infer_engine_factory_t* find_factory_by_engine(infer_engine_t engine) {
    for (int i = 0; i < mapping_count; i++) {
        if (engine_mappings[i].engine == engine) {
            return engine_mappings[i].factory;
        }
    }
    return NULL;
}

static void add_engine_mapping(infer_engine_t engine, const infer_engine_factory_t* factory) {
    if (mapping_count < 64) {
        engine_mappings[mapping_count].engine = engine;
        engine_mappings[mapping_count].factory = factory;
        mapping_count++;
    }
}

static void remove_engine_mapping(infer_engine_t engine) {
    for (int i = 0; i < mapping_count; i++) {
        if (engine_mappings[i].engine == engine) {
            // 移动后续元素
            for (int j = i; j < mapping_count - 1; j++) {
                engine_mappings[j] = engine_mappings[j + 1];
            }
            mapping_count--;
            break;
        }
    }
}

static int try_load_backend_from_plugins(infer_backend_type_e backend) {
    initialize_global_plugin_factory();
    if (!global_plugin_factory) return -1;
    
    // 首先发现插件（如果还没有）
    static bool plugins_discovered = false;
    if (!plugins_discovered) {
        infer_engine_discover_plugins();
        plugins_discovered = true;
    }
    
    // 查找匹配的推理引擎插件
    char** plugin_names = NULL;
    uint32_t plugin_count = 0;
    
    if (plugin_factory_list(global_plugin_factory, &plugin_names, &plugin_count) != 0) {
        return -1;
    }
    
    for (uint32_t i = 0; i < plugin_count; i++) {
        plugin_t plugin = plugin_factory_get(global_plugin_factory, plugin_names[i]);
        if (plugin) {
            const plugin_info_t* info = plugin_get_info(plugin);
            if (info->type == PLUGIN_TYPE_INFERENCE_ENGINE) {
                // 初始化插件
                plugin_status_e status = plugin_get_status(plugin);
                if (status != PLUGIN_STATUS_INITIALIZED) {
                    if (plugin_initialize(plugin, NULL) != 0) {
                        continue;
                    }
                }
                
                // 检查是否匹配所需的后端
                const InferEngineFactory* factory = plugin_get_inference_engine_factory(plugin);
                if (factory && factory->backend == backend) {
                    printf("找到匹配的插件后端: %s -> %d\n", info->name, backend);
                    int result = infer_engine_register_factory(factory);
                    
                    // 清理
                    for (uint32_t j = 0; j < plugin_count; j++) {
                        free(plugin_names[j]);
                    }
                    free(plugin_names);
                    
                    return result == 0 || result == -2 ? 0 : -1; // -2 表示已存在，也算成功
                }
            }
        }
    }
    
    // 清理
    for (uint32_t i = 0; i < plugin_count; i++) {
        free(plugin_names[i]);
    }
    free(plugin_names);
    
    return -1; // 未找到匹配的插件
}

static void initialize_global_plugin_factory(void) {
    if (!global_plugin_factory) {
        global_plugin_factory = plugin_factory_create();
        if (global_plugin_factory) {
            printf("全局插件工厂初始化完成\n");
        }
    }
}

int infer_engine_load_model(infer_engine_t engine, const char* model_path, const void* model_data, size_t model_size) {
    if (!engine || !model_path) return -1;
    
    const infer_engine_factory_t* factory = find_factory_by_engine(engine);
    if (!factory || !factory->ops->load_model) return -1;
    
    return factory->ops->load_model(engine, model_path, model_data, model_size);
}

int infer_engine_unload_model(infer_engine_t engine) {
    if (!engine) return -1;
    
    const infer_engine_factory_t* factory = find_factory_by_engine(engine);
    if (!factory || !factory->ops->unload_model) return -1;
    
    return factory->ops->unload_model(engine);
}

int infer_engine_infer(infer_engine_t engine, const tensor_t* inputs, uint32_t input_count,
                       tensor_t* outputs, uint32_t output_count) {
    if (!engine || !inputs || !outputs) return -1;
    
    const infer_engine_factory_t* factory = find_factory_by_engine(engine);
    if (!factory || !factory->ops->infer) return -1;
    
    return factory->ops->infer(engine, inputs, input_count, outputs, output_count);
}

infer_backend_type_e infer_engine_get_backend_type_from_engine(infer_engine_t engine) {
    const infer_engine_factory_t* factory = find_factory_by_engine(engine);
    if (factory) {
        return factory->backend;
    }
    return INFER_BACKEND_UNKNOWN;
}

// 清理函数，程序退出时调用
__attribute__((destructor))
static void cleanup_factories(void) {
    for (int i = 0; i < factory_count; i++) {
        free(registered_factories[i]);
    }
    factory_count = 0;
    
    if (global_plugin_factory) {
        plugin_factory_destroy(global_plugin_factory);
        global_plugin_factory = NULL;
    }
} 