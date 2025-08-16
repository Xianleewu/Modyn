#include "core/plugin_factory.h"
#include "core/inference_engine.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

/**
 * @brief 插件内部结构
 */
struct Plugin {
    plugin_info_t info;
    plugin_interface_t interface;
    void* handle;           // 动态库句柄
    char* library_path;     // 库文件路径
    bool is_loaded;         // 是否已加载
    bool is_initialized;    // 是否已初始化
    pthread_mutex_t mutex;  // 插件互斥锁
};

/**
 * @brief 插件工厂内部结构
 */
struct PluginFactory {
    plugin_t* plugins;          // 插件数组
    uint32_t plugin_count;      // 插件数量
    uint32_t plugin_capacity;   // 插件容量
    
    char** search_paths;        // 搜索路径数组
    uint32_t path_count;        // 路径数量
    uint32_t path_capacity;     // 路径容量
    
    plugin_load_callback_t load_callback;  // 加载回调
    void* callback_user_data;               // 回调用户数据
    
    pthread_mutex_t mutex;      // 工厂互斥锁
};

// 插件入口点函数类型
typedef const plugin_interface_t* (*plugin_entry_point_t)(void);
typedef const plugin_info_t* (*plugin_info_entry_t)(void);

// 前向声明
static plugin_t create_plugin_from_library(const char* library_path);
static int load_plugin_library(plugin_t plugin);
static void unload_plugin_library(plugin_t plugin);
static bool is_valid_plugin_file(const char* filepath);
static char* get_plugin_name_from_path(const char* filepath);
static void notify_plugin_loaded(plugin_factory_t factory, const char* plugin_name, plugin_status_e status);

plugin_factory_t plugin_factory_create(void) {
    plugin_factory_t factory = calloc(1, sizeof(struct PluginFactory));
    if (!factory) {
        LOG_ERROR("Failed to allocate plugin factory");
        return NULL;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&factory->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize factory mutex");
        free(factory);
        return NULL;
    }
    
    // 初始化插件数组
    factory->plugin_capacity = 16;
    factory->plugins = calloc(factory->plugin_capacity, sizeof(plugin_t));
    if (!factory->plugins) {
        LOG_ERROR("Failed to allocate plugins array");
        pthread_mutex_destroy(&factory->mutex);
        free(factory);
        return NULL;
    }
    
    // 初始化搜索路径数组
    factory->path_capacity = 8;
    factory->search_paths = calloc(factory->path_capacity, sizeof(char*));
    if (!factory->search_paths) {
        LOG_ERROR("Failed to allocate search paths array");
        free(factory->plugins);
        pthread_mutex_destroy(&factory->mutex);
        free(factory);
        return NULL;
    }
    
    LOG_INFO("插件工厂创建成功");
    return factory;
}

void plugin_factory_destroy(plugin_factory_t factory) {
    if (!factory) return;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 卸载所有插件
    for (uint32_t i = 0; i < factory->plugin_count; i++) {
        if (factory->plugins[i]) {
            plugin_factory_unload(factory, factory->plugins[i]);
        }
    }
    
    // 释放插件数组
    free(factory->plugins);
    
    // 释放搜索路径
    for (uint32_t i = 0; i < factory->path_count; i++) {
        free(factory->search_paths[i]);
    }
    free(factory->search_paths);
    
    pthread_mutex_unlock(&factory->mutex);
    pthread_mutex_destroy(&factory->mutex);
    
    free(factory);
    LOG_INFO("插件工厂已销毁");
}

int plugin_factory_add_search_path(plugin_factory_t factory, const char* path) {
    if (!factory || !path) return -1;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 检查路径是否已存在
    for (uint32_t i = 0; i < factory->path_count; i++) {
        if (strcmp(factory->search_paths[i], path) == 0) {
            pthread_mutex_unlock(&factory->mutex);
            LOG_DEBUG("搜索路径已存在: %s", path);
            return 0; // 已存在，不算错误
        }
    }
    
    // 扩展数组容量
    if (factory->path_count >= factory->path_capacity) {
        uint32_t new_capacity = factory->path_capacity * 2;
        char** new_paths = realloc(factory->search_paths, new_capacity * sizeof(char*));
        if (!new_paths) {
            pthread_mutex_unlock(&factory->mutex);
            LOG_ERROR("Failed to expand search paths array");
            return -1;
        }
        factory->search_paths = new_paths;
        factory->path_capacity = new_capacity;
    }
    
    // 添加新路径
    factory->search_paths[factory->path_count] = strdup(path);
    if (!factory->search_paths[factory->path_count]) {
        pthread_mutex_unlock(&factory->mutex);
        LOG_ERROR("Failed to duplicate search path");
        return -1;
    }
    
    factory->path_count++;
    pthread_mutex_unlock(&factory->mutex);
    
    LOG_INFO("添加插件搜索路径: %s", path);
    return 0;
}

int plugin_factory_remove_search_path(plugin_factory_t factory, const char* path) {
    if (!factory || !path) return -1;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 查找并移除路径
    for (uint32_t i = 0; i < factory->path_count; i++) {
        if (strcmp(factory->search_paths[i], path) == 0) {
            free(factory->search_paths[i]);
            
            // 移动后续元素
            for (uint32_t j = i; j < factory->path_count - 1; j++) {
                factory->search_paths[j] = factory->search_paths[j + 1];
            }
            
            factory->path_count--;
            pthread_mutex_unlock(&factory->mutex);
            LOG_INFO("移除插件搜索路径: %s", path);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&factory->mutex);
    LOG_WARN("搜索路径未找到: %s", path);
    return -1;
}

int plugin_factory_discover(plugin_factory_t factory, plugin_discovery_callback_t callback, void* user_data) {
    if (!factory) return -1;
    
    int discovered_count = 0;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 遍历所有搜索路径
    for (uint32_t i = 0; i < factory->path_count; i++) {
        const char* search_path = factory->search_paths[i];
        
        DIR* dir = opendir(search_path);
        if (!dir) {
            LOG_WARN("无法打开目录: %s (%s)", search_path, strerror(errno));
            continue;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            // 跳过 . 和 ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            // 构建完整路径
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", search_path, entry->d_name);
            
            // 检查是否为有效的插件文件
            if (is_valid_plugin_file(full_path)) {
                LOG_DEBUG("发现插件文件: %s", full_path);
                
                // 创建临时插件来获取信息
                plugin_t temp_plugin = create_plugin_from_library(full_path);
                if (temp_plugin) {
                    discovered_count++;
                    
                    // 调用发现回调
                    if (callback) {
                        callback(full_path, &temp_plugin->info, user_data);
                    }
                    
                    // 销毁临时插件
                    unload_plugin_library(temp_plugin);
                    pthread_mutex_destroy(&temp_plugin->mutex);
                    free(temp_plugin->library_path);
                    free(temp_plugin);
                }
            }
        }
        
        closedir(dir);
    }
    
    pthread_mutex_unlock(&factory->mutex);
    
    LOG_INFO("插件发现完成，找到 %d 个插件", discovered_count);
    return discovered_count;
}

plugin_t plugin_factory_load(plugin_factory_t factory, const char* plugin_name) {
    if (!factory || !plugin_name) return NULL;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 检查插件是否已加载
    for (uint32_t i = 0; i < factory->plugin_count; i++) {
        if (factory->plugins[i] && 
            strcmp(factory->plugins[i]->info.name, plugin_name) == 0) {
            pthread_mutex_unlock(&factory->mutex);
            LOG_DEBUG("插件已加载: %s", plugin_name);
            return factory->plugins[i];
        }
    }
    
    // 在搜索路径中查找插件
    plugin_t plugin = NULL;
    for (uint32_t i = 0; i < factory->path_count; i++) {
        const char* search_path = factory->search_paths[i];
        
        DIR* dir = opendir(search_path);
        if (!dir) continue;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", search_path, entry->d_name);
            
            if (is_valid_plugin_file(full_path)) {
                char* name = get_plugin_name_from_path(full_path);
                if (name && strcmp(name, plugin_name) == 0) {
                    plugin = plugin_factory_load_from_file(factory, full_path);
                    free(name);
                    break;
                }
                free(name);
            }
        }
        
        closedir(dir);
        if (plugin) break;
    }
    
    pthread_mutex_unlock(&factory->mutex);
    
    if (plugin) {
        notify_plugin_loaded(factory, plugin_name, PLUGIN_STATUS_LOADED);
        LOG_INFO("插件加载成功: %s", plugin_name);
    } else {
        LOG_ERROR("插件未找到: %s", plugin_name);
    }
    
    return plugin;
}

plugin_t plugin_factory_load_from_file(plugin_factory_t factory, const char* plugin_path) {
    if (!factory || !plugin_path) return NULL;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 检查文件是否存在
    if (access(plugin_path, F_OK) != 0) {
        pthread_mutex_unlock(&factory->mutex);
        LOG_ERROR("插件文件不存在: %s", plugin_path);
        return NULL;
    }
    
    // 创建插件
    plugin_t plugin = create_plugin_from_library(plugin_path);
    if (!plugin) {
        pthread_mutex_unlock(&factory->mutex);
        LOG_ERROR("创建插件失败: %s", plugin_path);
        return NULL;
    }
    
    // 加载插件库
    if (load_plugin_library(plugin) != 0) {
        pthread_mutex_destroy(&plugin->mutex);
        free(plugin->library_path);
        free(plugin);
        pthread_mutex_unlock(&factory->mutex);
        LOG_ERROR("加载插件库失败: %s", plugin_path);
        return NULL;
    }
    
    // 扩展插件数组容量
    if (factory->plugin_count >= factory->plugin_capacity) {
        uint32_t new_capacity = factory->plugin_capacity * 2;
        plugin_t* new_plugins = realloc(factory->plugins, new_capacity * sizeof(plugin_t));
        if (!new_plugins) {
            unload_plugin_library(plugin);
            pthread_mutex_destroy(&plugin->mutex);
            free(plugin->library_path);
            free(plugin);
            pthread_mutex_unlock(&factory->mutex);
            LOG_ERROR("Failed to expand plugins array");
            return NULL;
        }
        factory->plugins = new_plugins;
        factory->plugin_capacity = new_capacity;
    }
    
    // 添加到工厂
    factory->plugins[factory->plugin_count] = plugin;
    factory->plugin_count++;
    
    pthread_mutex_unlock(&factory->mutex);
    
    LOG_INFO("从文件加载插件成功: %s", plugin_path);
    return plugin;
}

int plugin_factory_unload(plugin_factory_t factory, plugin_t plugin) {
    if (!factory || !plugin) return -1;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 查找并移除插件
    for (uint32_t i = 0; i < factory->plugin_count; i++) {
        if (factory->plugins[i] == plugin) {
            // 销毁插件
            if (plugin->is_initialized) {
                plugin_finalize(plugin);
            }
            
            unload_plugin_library(plugin);
            pthread_mutex_destroy(&plugin->mutex);
            free(plugin->library_path);
            free(plugin);
            
            // 移动后续元素
            for (uint32_t j = i; j < factory->plugin_count - 1; j++) {
                factory->plugins[j] = factory->plugins[j + 1];
            }
            
            factory->plugin_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&factory->mutex);
    
    LOG_INFO("插件卸载成功");
    return 0;
}

plugin_t plugin_factory_get(plugin_factory_t factory, const char* plugin_name) {
    if (!factory || !plugin_name) return NULL;
    
    pthread_mutex_lock(&factory->mutex);
    
    for (uint32_t i = 0; i < factory->plugin_count; i++) {
        if (factory->plugins[i] && 
            strcmp(factory->plugins[i]->info.name, plugin_name) == 0) {
            pthread_mutex_unlock(&factory->mutex);
            return factory->plugins[i];
        }
    }
    
    pthread_mutex_unlock(&factory->mutex);
    return NULL;
}

int plugin_factory_list(plugin_factory_t factory, char*** plugin_names, uint32_t* count) {
    if (!factory || !plugin_names || !count) return -1;
    
    pthread_mutex_lock(&factory->mutex);
    
    *count = factory->plugin_count;
    if (*count == 0) {
        *plugin_names = NULL;
        pthread_mutex_unlock(&factory->mutex);
        return 0;
    }
    
    *plugin_names = malloc(*count * sizeof(char*));
    if (!*plugin_names) {
        pthread_mutex_unlock(&factory->mutex);
        return -1;
    }
    
    for (uint32_t i = 0; i < *count; i++) {
        (*plugin_names)[i] = strdup(factory->plugins[i]->info.name);
    }
    
    pthread_mutex_unlock(&factory->mutex);
    return 0;
}

// 插件接口函数实现
const plugin_info_t* plugin_get_info(plugin_t plugin) {
    if (!plugin) return NULL;
    return &plugin->info;
}

const plugin_interface_t* plugin_get_interface(plugin_t plugin) {
    if (!plugin) return NULL;
    return &plugin->interface;
}

int plugin_initialize(plugin_t plugin, const void* config) {
    if (!plugin || plugin->is_initialized) return -1;
    
    pthread_mutex_lock(&plugin->mutex);
    
    int result = 0;
    if (plugin->interface.initialize) {
        result = plugin->interface.initialize((void*)config);
        if (result == 0) {
            plugin->is_initialized = true;
            plugin->info.status = PLUGIN_STATUS_INITIALIZED;
        } else {
            plugin->info.status = PLUGIN_STATUS_ERROR;
        }
    }
    
    pthread_mutex_unlock(&plugin->mutex);
    return result;
}

void plugin_finalize(plugin_t plugin) {
    if (!plugin || !plugin->is_initialized) return;
    
    pthread_mutex_lock(&plugin->mutex);
    
    if (plugin->interface.finalize) {
        plugin->interface.finalize();
    }
    
    plugin->is_initialized = false;
    plugin->info.status = PLUGIN_STATUS_LOADED;
    
    pthread_mutex_unlock(&plugin->mutex);
}

void* plugin_create_instance(plugin_t plugin, const void* config) {
    if (!plugin || !plugin->is_initialized) return NULL;
    
    if (plugin->interface.create_instance) {
        return plugin->interface.create_instance(config);
    }
    
    return NULL;
}

void plugin_destroy_instance(plugin_t plugin, void* instance) {
    if (!plugin || !instance) return;
    
    if (plugin->interface.destroy_instance) {
        plugin->interface.destroy_instance(instance);
    }
}

bool plugin_check_compatibility(plugin_t plugin, const char* requirement) {
    if (!plugin || !requirement) return false;
    
    if (plugin->interface.check_compatibility) {
        return plugin->interface.check_compatibility(requirement);
    }
    
    return true; // 默认兼容
}

int plugin_self_test(plugin_t plugin) {
    if (!plugin) return -1;
    
    if (plugin->interface.self_test) {
        return plugin->interface.self_test();
    }
    
    return 0; // 默认通过
}

plugin_status_e plugin_get_status(plugin_t plugin) {
    if (!plugin) return PLUGIN_STATUS_UNLOADED;
    return plugin->info.status;
}

void plugin_factory_set_load_callback(plugin_factory_t factory, plugin_load_callback_t callback, void* user_data) {
    if (!factory) return;
    
    pthread_mutex_lock(&factory->mutex);
    factory->load_callback = callback;
    factory->callback_user_data = user_data;
    pthread_mutex_unlock(&factory->mutex);
}

// 推理引擎插件相关函数
const InferEngineFactory* plugin_get_inference_engine_factory(plugin_t plugin) {
    if (!plugin || plugin->info.type != PLUGIN_TYPE_INFERENCE_ENGINE) return NULL;
    
    // 尝试获取推理引擎工厂
    if (plugin->interface.create_instance) {
        return (const InferEngineFactory*)plugin->interface.create_instance(NULL);
    }
    
    return NULL;
}

int plugin_factory_register_inference_engine(plugin_factory_t factory, InferBackendType backend, 
                                            const char* plugin_name) {
    if (!factory || !plugin_name) return -1;
    
    plugin_t plugin = plugin_factory_get(factory, plugin_name);
    if (!plugin) {
        plugin = plugin_factory_load(factory, plugin_name);
        if (!plugin) return -1;
    }
    
    if (plugin->info.type != PLUGIN_TYPE_INFERENCE_ENGINE) {
        LOG_ERROR("插件不是推理引擎类型: %s", plugin_name);
        return -1;
    }
    
    // 初始化插件
    if (!plugin->is_initialized) {
        if (plugin_initialize(plugin, NULL) != 0) {
            LOG_ERROR("插件初始化失败: %s", plugin_name);
            return -1;
        }
    }
    
    // 获取推理引擎工厂并注册
    const InferEngineFactory* engine_factory = plugin_get_inference_engine_factory(plugin);
    if (engine_factory) {
        return infer_engine_register_factory(engine_factory);
    }
    
    return -1;
}

InferEngine plugin_factory_create_inference_engine(plugin_factory_t factory, InferBackendType backend, 
                                                   const InferEngineConfig* config) {
    if (!factory || !config) return NULL;
    
    // 首先尝试使用已注册的工厂
    InferEngine engine = infer_engine_create(backend, config);
    if (engine) {
        return engine;
    }
    
    // 如果失败，尝试从插件中查找
    pthread_mutex_lock(&factory->mutex);
    
    for (uint32_t i = 0; i < factory->plugin_count; i++) {
        plugin_t plugin = factory->plugins[i];
        if (plugin && plugin->info.type == PLUGIN_TYPE_INFERENCE_ENGINE) {
            const InferEngineFactory* engine_factory = plugin_get_inference_engine_factory(plugin);
            if (engine_factory && engine_factory->backend == backend) {
                // 动态注册并创建
                if (infer_engine_register_factory(engine_factory) == 0) {
                    engine = infer_engine_create(backend, config);
                    break;
                }
            }
        }
    }
    
    pthread_mutex_unlock(&factory->mutex);
    return engine;
}

// 工具函数实现
static plugin_t create_plugin_from_library(const char* library_path) {
    plugin_t plugin = calloc(1, sizeof(struct Plugin));
    if (!plugin) return NULL;
    
    plugin->library_path = strdup(library_path);
    if (!plugin->library_path) {
        free(plugin);
        return NULL;
    }
    
    if (pthread_mutex_init(&plugin->mutex, NULL) != 0) {
        free(plugin->library_path);
        free(plugin);
        return NULL;
    }
    
    plugin->info.status = PLUGIN_STATUS_UNLOADED;
    plugin->is_loaded = false;
    plugin->is_initialized = false;
    
    return plugin;
}

static int load_plugin_library(plugin_t plugin) {
    if (!plugin) return -1;
    
    // 加载动态库
    plugin->handle = dlopen(plugin->library_path, RTLD_LAZY);
    if (!plugin->handle) {
        LOG_ERROR("加载动态库失败: %s - %s", plugin->library_path, dlerror());
        return -1;
    }
    
    // 获取插件信息入口点
    plugin_info_entry_t info_entry = (plugin_info_entry_t)dlsym(plugin->handle, "plugin_get_info");
    if (!info_entry) {
        LOG_ERROR("插件缺少信息入口点: %s", plugin->library_path);
        dlclose(plugin->handle);
        plugin->handle = NULL;
        return -1;
    }
    
    // 获取插件接口入口点
    plugin_entry_point_t entry_point = (plugin_entry_point_t)dlsym(plugin->handle, "plugin_get_interface");
    if (!entry_point) {
        LOG_ERROR("插件缺少接口入口点: %s", plugin->library_path);
        dlclose(plugin->handle);
        plugin->handle = NULL;
        return -1;
    }
    
    // 获取插件信息和接口
    const plugin_info_t* info = info_entry();
    const plugin_interface_t* interface = entry_point();
    
    if (!info || !interface) {
        LOG_ERROR("插件信息或接口获取失败: %s", plugin->library_path);
        dlclose(plugin->handle);
        plugin->handle = NULL;
        return -1;
    }
    
    // 复制插件信息
    plugin->info = *info;
    plugin->interface = *interface;
    plugin->info.status = PLUGIN_STATUS_LOADED;
    plugin->is_loaded = true;
    
    LOG_DEBUG("插件库加载成功: %s (%s)", plugin->info.name, plugin->library_path);
    return 0;
}

static void unload_plugin_library(plugin_t plugin) {
    if (!plugin || !plugin->handle) return;
    
    dlclose(plugin->handle);
    plugin->handle = NULL;
    plugin->is_loaded = false;
    plugin->info.status = PLUGIN_STATUS_UNLOADED;
    
    LOG_DEBUG("插件库卸载成功: %s", plugin->library_path);
}

static bool is_valid_plugin_file(const char* filepath) {
    if (!filepath) return false;
    
    // 检查文件扩展名
    const char* ext = strrchr(filepath, '.');
    if (!ext) return false;
    
    if (strcmp(ext, ".so") == 0 || strcmp(ext, ".dylib") == 0 || strcmp(ext, ".dll") == 0) {
        // 检查文件是否存在且可读
        return access(filepath, R_OK) == 0;
    }
    
    return false;
}

static char* get_plugin_name_from_path(const char* filepath) {
    if (!filepath) return NULL;
    
    const char* filename = strrchr(filepath, '/');
    if (!filename) filename = filepath;
    else filename++; // 跳过 '/'
    
    // 移除扩展名
    char* name = strdup(filename);
    if (!name) return NULL;
    
    char* dot = strrchr(name, '.');
    if (dot) *dot = '\0';
    
    // 移除 lib 前缀
    if (strncmp(name, "lib", 3) == 0) {
        char* new_name = strdup(name + 3);
        free(name);
        return new_name;
    }
    
    return name;
}

static void notify_plugin_loaded(plugin_factory_t factory, const char* plugin_name, plugin_status_e status) {
    if (!factory) return;
    
    pthread_mutex_lock(&factory->mutex);
    if (factory->load_callback) {
        factory->load_callback(plugin_name, status, factory->callback_user_data);
    }
    pthread_mutex_unlock(&factory->mutex);
}

// 版本管理函数实现
int plugin_version_compare(const plugin_version_t* v1, const plugin_version_t* v2) {
    if (!v1 || !v2) return 0;
    
    if (v1->major != v2->major) return v1->major - v2->major;
    if (v1->minor != v2->minor) return v1->minor - v2->minor;
    if (v1->patch != v2->patch) return v1->patch - v2->patch;
    
    return 0;
}

int plugin_version_parse(const char* version_str, plugin_version_t* version) {
    if (!version_str || !version) return -1;
    
    memset(version, 0, sizeof(plugin_version_t));
    
    int parsed = sscanf(version_str, "%u.%u.%u", &version->major, &version->minor, &version->patch);
    if (parsed < 1) return -1;
    
    // 查找构建信息
    const char* dash = strchr(version_str, '-');
    if (dash) {
        version->build = strdup(dash + 1);
    }
    
    return 0;
}

char* plugin_version_to_string(const plugin_version_t* version) {
    if (!version) return NULL;
    
    char* version_str = malloc(64);
    if (!version_str) return NULL;
    
    if (version->build) {
        snprintf(version_str, 64, "%u.%u.%u-%s", 
                 version->major, version->minor, version->patch, version->build);
    } else {
        snprintf(version_str, 64, "%u.%u.%u", 
                 version->major, version->minor, version->patch);
    }
    
    return version_str;
} 

int plugin_factory_get_available_backends(plugin_factory_t factory, InferBackendType** backends, uint32_t* count) {
    if (!factory || !backends || !count) {
        return -1;
    }
    
    *backends = NULL;
    *count = 0;
    
    // 分配临时数组
    InferBackendType* temp_backends = malloc(factory->plugin_count * sizeof(InferBackendType));
    if (!temp_backends) {
        return -1;
    }
    
    uint32_t backend_count = 0;
    
    pthread_mutex_lock(&factory->mutex);
    
    // 遍历所有已加载的插件
    for (uint32_t i = 0; i < factory->plugin_count; i++) {
        plugin_t plugin = factory->plugins[i];
        
        if (plugin && plugin->is_loaded && plugin->info.type == PLUGIN_TYPE_INFERENCE_ENGINE) {
            // 获取推理引擎工厂
            const InferEngineFactory* engine_factory = plugin_get_inference_engine_factory(plugin);
            if (engine_factory) {
                temp_backends[backend_count++] = engine_factory->backend;
            }
        }
    }
    
    pthread_mutex_unlock(&factory->mutex);
    
    if (backend_count > 0) {
        // 重新分配正确大小的数组
        *backends = malloc(backend_count * sizeof(InferBackendType));
        if (*backends) {
            memcpy(*backends, temp_backends, backend_count * sizeof(InferBackendType));
            *count = backend_count;
        } else {
            backend_count = 0;
        }
    }
    
    free(temp_backends);
    
    return backend_count > 0 ? 0 : -1;
} 