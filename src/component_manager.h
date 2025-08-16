#ifndef MODYN_COMPONENT_MANAGER_H
#define MODYN_COMPONENT_MANAGER_H

#include "modyn.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ======================== 组件类型定义 ======================== //

/**
 * @brief 组件类型枚举
 */
typedef enum __modyn_component_type {
  MODYN_COMPONENT_DEVICE = 0,      // 推理设备
  MODYN_COMPONENT_MEMORY_POOL,     // 内存池
  MODYN_COMPONENT_MODEL_LOADER,    // 模型加载器
  MODYN_COMPONENT_PIPELINE_NODE,   // Pipeline节点
  MODYN_COMPONENT_COUNT
} modyn_component_type_e;

/**
 * @brief 组件状态枚举
 */
typedef enum __modyn_component_status {
  MODYN_COMPONENT_LOADED = 0,      // 已加载
  MODYN_COMPONENT_ACTIVE,          // 活跃状态
  MODYN_COMPONENT_INACTIVE,        // 非活跃状态
  MODYN_COMPONENT_ERROR,           // 错误状态
  MODYN_COMPONENT_UNLOADED         // 已卸载
} modyn_component_status_e;

/**
 * @brief 组件来源枚举
 */
typedef enum __modyn_component_source {
  MODYN_COMPONENT_SOURCE_BUILTIN = 0,  // 内建组件
  MODYN_COMPONENT_SOURCE_PLUGIN,       // 插件组件
  MODYN_COMPONENT_SOURCE_DYNAMIC       // 动态加载
} modyn_component_source_e;

// ======================== 组件接口定义 ======================== //

/**
 * @brief 组件基础接口 - 所有组件都必须实现
 * 专注于组件管理功能，不涉及组件的内部生命周期
 */
typedef struct __modyn_component_interface {
  // 基本信息
  const char *name;                    // 组件名称
  const char *version;                 // 版本字符串
  modyn_component_type_e type;         // 组件类型
  modyn_component_source_e source;     // 组件来源
  
  // 组件管理功能（必需）
  modyn_status_e (*query)(const void *component, void *query_info);  // 组件信息查询
  modyn_component_status_e (*get_status)(const void *component);     // 组件状态查询
  
  // 能力查询（必需）
  int (*supports_feature)(const void *component, const char *feature);  // 功能支持检查
  const char* (*get_capabilities)(const void *component);               // 能力描述
  
  // 私有数据 - 指向组件实例或驱动结构
  void *private_data;
} modyn_component_interface_t;

// ======================== 组件注册器定义 ======================== //

/**
 * @brief 组件注册器 - 用于自动注册组件
 */
typedef struct __modyn_component_registry {
  modyn_component_type_e type;         // 组件类型
  const char *type_name;               // 类型名称
  modyn_component_interface_t *interface; // 组件接口
  struct __modyn_component_registry *next; // 链表指针
} modyn_component_registry_t;

// ======================== 插件加载器定义 ======================== //

/**
 * @brief 插件信息
 */
typedef struct __modyn_plugin_info {
  char name[64];                       // 插件名称
  char version[32];                    // 插件版本
  char description[256];               // 插件描述
  char author[64];                     // 作者
  char license[32];                    // 许可证
  char file_path[256];                 // 插件文件路径
  void *handle;                        // 动态库句柄
  modyn_component_source_e source;     // 来源类型
  time_t load_time;                    // 加载时间
} modyn_plugin_info_t;

/**
 * @brief 插件加载器接口
 */
typedef struct __modyn_plugin_loader {
  // 插件管理
  modyn_status_e (*load_plugin)(const char *plugin_path, modyn_plugin_info_t *info);
  modyn_status_e (*unload_plugin)(const char *plugin_name);
  modyn_status_e (*reload_plugin)(const char *plugin_name);
  
  // 插件查询
  modyn_status_e (*list_plugins)(modyn_plugin_info_t *plugins, size_t max_plugins, size_t *count);
  modyn_status_e (*get_plugin_info)(const char *plugin_name, modyn_plugin_info_t *info);
  
  // 插件验证
  modyn_status_e (*validate_plugin)(const char *plugin_path, int *is_valid);
  modyn_status_e (*check_compatibility)(const char *plugin_path, int *is_compatible);
} modyn_plugin_loader_t;

// ======================== 组件管理器定义 ======================== //

/**
 * @brief 组件管理器
 */
typedef struct __modyn_component_manager {
  // 组件注册表
  modyn_component_registry_t *registries[MODYN_COMPONENT_COUNT];
  size_t component_counts[MODYN_COMPONENT_COUNT];
  
  // 插件管理
  modyn_plugin_loader_t plugin_loader;
  modyn_plugin_info_t *loaded_plugins;
  size_t num_plugins;
  size_t max_plugins;
  
  // 配置
  char plugin_search_paths[8][256];    // 插件搜索路径
  size_t num_search_paths;
  int auto_discovery_enabled;          // 是否启用自动发现
  int hot_reload_enabled;              // 是否启用热重载
} modyn_component_manager_t;

// ======================== 自动注册宏定义 ======================== //

/**
 * @brief 内建组件自动注册宏
 * 使用方式：MODYN_REGISTER_BUILTIN_COMPONENT(component_type, component_name, component_interface)
 */
#define MODYN_REGISTER_BUILTIN_COMPONENT(type, name, interface) \
  __attribute__((constructor)) \
  static void __modyn_register_##type##_##name##_component(void) { \
    modyn_register_component(type, #name, interface, MODYN_COMPONENT_SOURCE_BUILTIN); \
  }

/**
 * @brief 简化的内建组件注册宏
 * 使用方式：MODYN_REGISTER_COMPONENT_SIMPLE(component_type, component_name, component_interface)
 */
#define MODYN_REGISTER_COMPONENT_SIMPLE(type, name, interface) \
  __attribute__((constructor)) \
  static void __modyn_register_##type##_##name##_simple(void) { \
    modyn_register_component(type, name, interface, MODYN_COMPONENT_SOURCE_BUILTIN); \
  }

/**
 * @brief 最简化的组件注册宏 - 直接传递字符串
 */
#define MODYN_REGISTER_COMPONENT_STRING(type, name_str, interface) \
  __attribute__((constructor)) \
  static void __modyn_register_##type##_component(void) { \
    modyn_register_component(type, name_str, interface, MODYN_COMPONENT_SOURCE_BUILTIN); \
  }

/**
 * @brief 插件组件注册宏
 * 使用方式：MODYN_REGISTER_PLUGIN_COMPONENT(component_type, component_name, component_interface)
 */
#define MODYN_REGISTER_PLUGIN_COMPONENT(type, name, interface) \
  __attribute__((constructor)) \
  static void __modyn_register_##type##_##name(void) { \
    modyn_register_component(type, name, interface, MODYN_COMPONENT_SOURCE_PLUGIN); \
  }

/**
 * @brief 组件工厂函数类型
 */
typedef void* (*modyn_component_factory_fn)(const char *name, void *config);

/**
 * @brief 组件工厂注册宏
 * 使用方式：MODYN_REGISTER_COMPONENT_FACTORY(component_type, factory_name, factory_function)
 */
#define MODYN_REGISTER_COMPONENT_FACTORY(type, factory_name, factory_fn) \
  __attribute__((constructor)) \
  static void __modyn_register_##type##_##factory_name(void) { \
    modyn_register_component_factory(type, factory_name, factory_fn); \
  }

// ======================== 公共API函数 ======================== //

/**
 * @brief 初始化组件管理器
 *
 * @param config 配置参数
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_component_manager_init(const void *config);

/**
 * @brief 关闭组件管理器
 *
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_component_manager_shutdown(void);

/**
 * @brief 注册组件
 *
 * @param type 组件类型
 * @param name 组件名称
 * @param interface 组件接口
 * @param source 组件来源
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_register_component(modyn_component_type_e type,
                                       const char *name,
                                       modyn_component_interface_t *interface,
                                       modyn_component_source_e source);

/**
 * @brief 注销组件
 *
 * @param type 组件类型
 * @param name 组件名称
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_unregister_component(modyn_component_type_e type,
                                         const char *name);

/**
 * @brief 查找组件
 *
 * @param type 组件类型
 * @param name 组件名称
 * @return modyn_component_interface_t* 组件接口，未找到返回NULL
 */
modyn_component_interface_t* modyn_find_component(modyn_component_type_e type,
                                                 const char *name);

/**
 * @brief 获取组件列表
 *
 * @param type 组件类型
 * @param components 组件接口数组
 * @param max_components 最大组件数量
 * @param count 实际找到的组件数量
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_get_components(modyn_component_type_e type,
                                   modyn_component_interface_t **components,
                                   size_t max_components,
                                   size_t *count);

/**
 * @brief 加载插件
 *
 * @param plugin_path 插件文件路径
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_load_plugin(const char *plugin_path);

/**
 * @brief 卸载插件
 *
 * @param plugin_name 插件名称
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_unload_plugin(const char *plugin_name);

/**
 * @brief 重新加载插件
 *
 * @param plugin_name 插件名称
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_reload_plugin(const char *plugin_name);

/**
 * @brief 获取已加载的插件列表
 *
 * @param plugins 插件信息数组
 * @param max_plugins 最大插件数量
 * @param count 实际插件数量
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_list_loaded_plugins(modyn_plugin_info_t *plugins,
                                        size_t max_plugins,
                                        size_t *count);

/**
 * @brief 设置插件搜索路径
 *
 * @param paths 搜索路径数组
 * @param num_paths 路径数量
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_set_plugin_search_paths(const char **paths, size_t num_paths);

/**
 * @brief 启用/禁用插件自动发现
 *
 * @param enabled 是否启用
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_set_plugin_auto_discovery(int enabled);

/**
 * @brief 扫描并加载插件目录中的所有插件
 *
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_scan_and_load_plugins(void);

/**
 * @brief 获取组件管理器统计信息
 *
 * @param total_components 总组件数量
 * @param builtin_components 内建组件数量
 * @param plugin_components 插件组件数量
 * @param loaded_plugins 已加载插件数量
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_get_component_manager_stats(size_t *total_components,
                                                size_t *builtin_components,
                                                size_t *plugin_components,
                                                size_t *loaded_plugins);

/**
 * @brief 格式化打印所有已注册组件信息（类似FFmpeg格式）
 *
 * @param output_format 输出格式 ("text", "json", "xml", "csv")
 * @param show_details 是否显示详细信息
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_print_registered_components(const char *output_format, int show_details);

/**
 * @brief 获取组件信息的JSON格式字符串
 *
 * @param json_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_get_components_json(char *json_buffer, size_t buffer_size);

/**
 * @brief 获取组件信息的XML格式字符串
 *
 * @param xml_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_get_components_xml(char *xml_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // MODYN_COMPONENT_MANAGER_H
