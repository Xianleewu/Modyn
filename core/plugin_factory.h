#ifndef MODYN_CORE_PLUGIN_FACTORY_H
#define MODYN_CORE_PLUGIN_FACTORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/inference_engine.h"
#include "multimodal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 插件类型枚举
 */
typedef enum {
    PLUGIN_TYPE_UNKNOWN = 0,        /**< 未知类型 */
    PLUGIN_TYPE_INFERENCE_ENGINE,   /**< 推理引擎插件 */
    PLUGIN_TYPE_PREPROCESSOR,       /**< 预处理器插件 */
    PLUGIN_TYPE_POSTPROCESSOR,      /**< 后处理器插件 */
    PLUGIN_TYPE_CONVERTER,          /**< 转换器插件 */
    PLUGIN_TYPE_CODEC,              /**< 编解码器插件 */
    PLUGIN_TYPE_CUSTOM              /**< 自定义插件 */
} plugin_type_e;

/**
 * @brief 插件状态枚举
 */
typedef enum {
    PLUGIN_STATUS_UNLOADED = 0,     /**< 未加载 */
    PLUGIN_STATUS_LOADING,          /**< 加载中 */
    PLUGIN_STATUS_LOADED,           /**< 已加载 */
    PLUGIN_STATUS_INITIALIZED,      /**< 已初始化 */
    PLUGIN_STATUS_ERROR,            /**< 错误状态 */
    PLUGIN_STATUS_DEPRECATED        /**< 已废弃 */
} plugin_status_e;

/**
 * @brief 插件版本信息
 */
typedef struct {
    uint32_t major;                 /**< 主版本号 */
    uint32_t minor;                 /**< 次版本号 */
    uint32_t patch;                 /**< 补丁版本号 */
    char* build;                    /**< 构建信息 */
} plugin_version_t;

/**
 * @brief 插件依赖信息
 */
typedef struct {
    char* name;                     /**< 依赖名称 */
    plugin_version_t min_version;   /**< 最小版本 */
    plugin_version_t max_version;   /**< 最大版本 */
    bool required;                  /**< 是否必需 */
} plugin_dependency_t;

/**
 * @brief 插件信息
 */
typedef struct {
    char* name;                     /**< 插件名称 */
    char* description;              /**< 插件描述 */
    char* author;                   /**< 作者 */
    char* license;                  /**< 许可证 */
    char* homepage;                 /**< 主页 */
    plugin_version_t version;       /**< 版本信息 */
    plugin_type_e type;             /**< 插件类型 */
    plugin_status_e status;         /**< 插件状态 */
    char* library_path;             /**< 库文件路径 */
    void* handle;                   /**< 库句柄 */
    plugin_dependency_t* dependencies; /**< 依赖列表 */
    uint32_t dependency_count;      /**< 依赖数量 */
    uint64_t load_time;             /**< 加载时间 */
    uint64_t init_time;             /**< 初始化时间 */
    void* user_data;                /**< 用户数据 */
} plugin_info_t;

/**
 * @brief 插件接口
 */
typedef struct {
    /**
     * @brief 获取插件信息
     */
    const plugin_info_t* (*get_info)(void);
    
    /**
     * @brief 初始化插件
     */
    int (*initialize)(void* config);
    
    /**
     * @brief 销毁插件
     */
    void (*finalize)(void);
    
    /**
     * @brief 获取插件实例
     */
    void* (*create_instance)(const void* config);
    
    /**
     * @brief 销毁插件实例
     */
    void (*destroy_instance)(void* instance);
    
    /**
     * @brief 获取插件功能列表
     */
    const char** (*get_capabilities)(uint32_t* count);
    
    /**
     * @brief 检查插件兼容性
     */
    bool (*check_compatibility)(const char* requirement);
    
    /**
     * @brief 插件自检
     */
    int (*self_test)(void);
    
    /**
     * @brief 获取插件配置
     */
    const char* (*get_config_schema)(void);
    
    /**
     * @brief 插件控制接口
     */
    int (*control)(const char* command, const void* params, void* result);
    
} plugin_interface_t;

/**
 * @brief 插件工厂句柄
 */
typedef struct PluginFactory* plugin_factory_t;

/**
 * @brief 插件句柄
 */
typedef struct Plugin* plugin_t;

/**
 * @brief 插件加载回调函数
 */
typedef void (*plugin_load_callback_t)(const char* plugin_name, plugin_status_e status, void* user_data);

/**
 * @brief 插件发现回调函数
 */
typedef void (*plugin_discovery_callback_t)(const char* plugin_path, const plugin_info_t* info, void* user_data);

/**
 * @brief 创建插件工厂
 * 
 * @return PluginFactory 插件工厂实例
 */
plugin_factory_t plugin_factory_create(void);

/**
 * @brief 销毁插件工厂
 * 
 * @param factory 插件工厂实例
 */
void plugin_factory_destroy(plugin_factory_t factory);

/**
 * @brief 注册插件搜索路径
 * 
 * @param factory 插件工厂实例
 * @param path 搜索路径
 * @return int 0成功，其他失败
 */
int plugin_factory_add_search_path(plugin_factory_t factory, const char* path);

/**
 * @brief 移除插件搜索路径
 * 
 * @param factory 插件工厂实例
 * @param path 搜索路径
 * @return int 0成功，其他失败
 */
int plugin_factory_remove_search_path(plugin_factory_t factory, const char* path);

/**
 * @brief 发现插件
 * 
 * @param factory 插件工厂实例
 * @param callback 发现回调函数
 * @param user_data 用户数据
 * @return int 发现的插件数量
 */
int plugin_factory_discover(plugin_factory_t factory, plugin_discovery_callback_t callback, void* user_data);

/**
 * @brief 加载插件
 * 
 * @param factory 插件工厂实例
 * @param plugin_name 插件名称
 * @return Plugin 插件实例
 */
plugin_t plugin_factory_load(plugin_factory_t factory, const char* plugin_name);

/**
 * @brief 从文件加载插件
 * 
 * @param factory 插件工厂实例
 * @param plugin_path 插件文件路径
 * @return Plugin 插件实例
 */
plugin_t plugin_factory_load_from_file(plugin_factory_t factory, const char* plugin_path);

/**
 * @brief 卸载插件
 * 
 * @param factory 插件工厂实例
 * @param plugin 插件实例
 * @return int 0成功，其他失败
 */
int plugin_factory_unload(plugin_factory_t factory, plugin_t plugin);

/**
 * @brief 获取插件
 * 
 * @param factory 插件工厂实例
 * @param plugin_name 插件名称
 * @return Plugin 插件实例
 */
plugin_t plugin_factory_get(plugin_factory_t factory, const char* plugin_name);

/**
 * @brief 列出所有插件
 * 
 * @param factory 插件工厂实例
 * @param plugin_names 插件名称数组输出
 * @param count 插件数量输出
 * @return int 0成功，其他失败
 */
int plugin_factory_list(plugin_factory_t factory, char*** plugin_names, uint32_t* count);

/**
 * @brief 获取插件信息
 * 
 * @param plugin 插件实例
 * @return const PluginInfo* 插件信息
 */
const plugin_info_t* plugin_get_info(plugin_t plugin);

/**
 * @brief 获取插件接口
 * 
 * @param plugin 插件实例
 * @return const PluginInterface* 插件接口
 */
const plugin_interface_t* plugin_get_interface(plugin_t plugin);

/**
 * @brief 初始化插件
 * 
 * @param plugin 插件实例
 * @param config 配置参数
 * @return int 0成功，其他失败
 */
int plugin_initialize(plugin_t plugin, const void* config);

/**
 * @brief 销毁插件
 * 
 * @param plugin 插件实例
 */
void plugin_finalize(plugin_t plugin);

/**
 * @brief 创建插件实例
 * 
 * @param plugin 插件实例
 * @param config 配置参数
 * @return void* 插件实例
 */
void* plugin_create_instance(plugin_t plugin, const void* config);

/**
 * @brief 销毁插件实例
 * 
 * @param plugin 插件实例
 * @param instance 插件实例
 */
void plugin_destroy_instance(plugin_t plugin, void* instance);

/**
 * @brief 检查插件兼容性
 * 
 * @param plugin 插件实例
 * @param requirement 兼容性要求
 * @return bool true兼容，false不兼容
 */
bool plugin_check_compatibility(plugin_t plugin, const char* requirement);

/**
 * @brief 插件自检
 * 
 * @param plugin 插件实例
 * @return int 0成功，其他失败
 */
int plugin_self_test(plugin_t plugin);

/**
 * @brief 获取插件状态
 * 
 * @param plugin 插件实例
 * @return PluginStatus 插件状态
 */
plugin_status_e plugin_get_status(plugin_t plugin);

/**
 * @brief 设置插件加载回调
 * 
 * @param factory 插件工厂实例
 * @param callback 加载回调函数
 * @param user_data 用户数据
 */
void plugin_factory_set_load_callback(plugin_factory_t factory, plugin_load_callback_t callback, void* user_data);

/**
 * @brief 插件版本比较
 * 
 * @param v1 版本1
 * @param v2 版本2
 * @return int <0表示v1<v2，0表示相等，>0表示v1>v2
 */
int plugin_version_compare(const plugin_version_t* v1, const plugin_version_t* v2);

/**
 * @brief 解析版本字符串
 * 
 * @param version_str 版本字符串
 * @param version 版本信息输出
 * @return int 0成功，其他失败
 */
int plugin_version_parse(const char* version_str, plugin_version_t* version);

/**
 * @brief 版本信息转字符串
 * 
 * @param version 版本信息
 * @return char* 版本字符串
 */
char* plugin_version_to_string(const plugin_version_t* version);

/**
 * @brief 检查依赖关系
 * 
 * @param factory 插件工厂实例
 * @param plugin 插件实例
 * @param missing_deps 缺失依赖输出
 * @param missing_count 缺失依赖数量输出
 * @return int 0成功，其他失败
 */
int plugin_check_dependencies(plugin_factory_t factory, plugin_t plugin, 
                             char*** missing_deps, uint32_t* missing_count);

/**
 * @brief 解析插件配置
 * 
 * @param plugin 插件实例
 * @param config_json JSON配置字符串
 * @param config 配置输出
 * @return int 0成功，其他失败
 */
int plugin_parse_config(plugin_t plugin, const char* config_json, void** config);

/**
 * @brief 验证插件配置
 * 
 * @param plugin 插件实例
 * @param config 配置
 * @return bool true有效，false无效
 */
bool plugin_validate_config(plugin_t plugin, const void* config);

/**
 * @brief 获取推理引擎工厂
 * 
 * @param plugin 插件实例
 * @return const InferEngineFactory* 推理引擎工厂
 */
const InferEngineFactory* plugin_get_inference_engine_factory(plugin_t plugin);

/**
 * @brief 注册推理引擎插件
 * 
 * @param factory 插件工厂实例
 * @param backend 后端类型
 * @param plugin_name 插件名称
 * @return int 0成功，其他失败
 */
int plugin_factory_register_inference_engine(plugin_factory_t factory, InferBackendType backend, 
                                            const char* plugin_name);

/**
 * @brief 创建推理引擎
 * 
 * @param factory 插件工厂实例
 * @param backend 后端类型
 * @param config 配置参数
 * @return InferEngine 推理引擎实例
 */
InferEngine plugin_factory_create_inference_engine(plugin_factory_t factory, InferBackendType backend, 
                                                   const InferEngineConfig* config);

/**
 * @brief 获取可用后端
 * 
 * @param factory 插件工厂实例
 * @param backends 后端类型数组输出
 * @param count 后端数量输出
 * @return int 0成功，其他失败
 */
int plugin_factory_get_available_backends(plugin_factory_t factory, InferBackendType** backends, uint32_t* count);

/**
 * @brief 插件热加载
 * 
 * @param factory 插件工厂实例
 * @param plugin_name 插件名称
 * @return int 0成功，其他失败
 */
int plugin_factory_hot_reload(plugin_factory_t factory, const char* plugin_name);

/**
 * @brief 插件性能监控
 * 
 * @param plugin 插件实例
 * @param enable 是否启用监控
 * @return int 0成功，其他失败
 */
int plugin_set_performance_monitoring(plugin_t plugin, bool enable);

/**
 * @brief 获取插件性能统计
 * 
 * @param plugin 插件实例
 * @param stats 统计信息输出
 * @return int 0成功，其他失败
 */
int plugin_get_performance_stats(plugin_t plugin, void** stats);

/**
 * @brief 插件调试信息
 * 
 * @param plugin 插件实例
 */
void plugin_print_debug(plugin_t plugin);

/**
 * @brief 插件工厂调试信息
 * 
 * @param factory 插件工厂实例
 */
void plugin_factory_print_debug(plugin_factory_t factory);

/**
 * @brief 插件完整性检查
 * 
 * @param plugin 插件实例
 * @return bool true完整，false损坏
 */
bool plugin_integrity_check(plugin_t plugin);

/**
 * @brief 插件安全检查
 * 
 * @param plugin 插件实例
 * @return bool true安全，false不安全
 */
bool plugin_security_check(plugin_t plugin);

#ifdef __cplusplus
}
#endif

#endif // MODYN_CORE_PLUGIN_FACTORY_H 