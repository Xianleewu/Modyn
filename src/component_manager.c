#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "component_manager.h"

// ======================== 全局组件管理器实例 ======================== //

static modyn_component_manager_t g_component_manager = {0};
static int g_initialized = 0;

// ======================== 内部函数声明 ======================== //

static modyn_status_e validate_plugin_file(const char *plugin_path);
static modyn_status_e check_plugin_compatibility(const char *plugin_path);
static modyn_status_e load_plugin_symbols(void *handle, modyn_plugin_info_t *info);
static modyn_status_e scan_directory_for_plugins(const char *dir_path);
static void cleanup_plugin_info(modyn_plugin_info_t *info);

// 格式化组件信息打印相关函数声明
static modyn_status_e print_components_text(int show_details);
static modyn_status_e print_components_json(int show_details);
static modyn_status_e print_components_xml(int show_details);
static modyn_status_e print_components_csv(int show_details);
static modyn_status_e generate_components_json(char *json_buffer, size_t buffer_size);
static modyn_status_e generate_components_xml(char *xml_buffer, size_t buffer_size);
static const char* get_component_type_name(int type);
static const char* get_component_source_name(modyn_component_source_e source);
static const char* get_component_status_name(modyn_component_status_e status);
static void test_component_features(modyn_component_interface_t *comp);
static const char* escape_csv_field(const char *field);

// ======================== 插件加载器实现 ======================== //

// 加载插件
static modyn_status_e plugin_loader_load_plugin(const char *plugin_path, modyn_plugin_info_t *info) {
  if (!plugin_path || !info) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 验证插件文件
  modyn_status_e status = validate_plugin_file(plugin_path);
  if (status != MODYN_SUCCESS) return status;
  
  // 检查兼容性
  int is_compatible = 0;
  status = check_plugin_compatibility(plugin_path);
  if (status != MODYN_SUCCESS) return status;
  
  // 加载动态库
  void *handle = dlopen(plugin_path, RTLD_LAZY | RTLD_GLOBAL);
  if (!handle) {
    printf("Failed to load plugin %s: %s\n", plugin_path, dlerror());
    return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
  }
  
  // 初始化插件信息
  memset(info, 0, sizeof(modyn_plugin_info_t));
  strncpy(info->file_path, plugin_path, sizeof(info->file_path)-1);
  info->handle = handle;
  info->source = MODYN_COMPONENT_SOURCE_PLUGIN;
  info->load_time = time(NULL);
  
  // 加载插件符号
  status = load_plugin_symbols(handle, info);
  if (status != MODYN_SUCCESS) {
    dlclose(handle);
    cleanup_plugin_info(info);
    return status;
  }
  
  printf("✓ Plugin loaded: %s (v%s)\n", info->name, info->version);
  return MODYN_SUCCESS;
}

// 卸载插件
static modyn_status_e plugin_loader_unload_plugin(const char *plugin_name) {
  if (!plugin_name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 查找插件
  for (size_t i = 0; i < g_component_manager.num_plugins; ++i) {
    if (strcmp(g_component_manager.loaded_plugins[i].name, plugin_name) == 0) {
      modyn_plugin_info_t *plugin = &g_component_manager.loaded_plugins[i];
      
      // 关闭动态库
      if (plugin->handle) {
        dlclose(plugin->handle);
        plugin->handle = NULL;
      }
      
      // 从列表中移除
      for (size_t j = i; j < g_component_manager.num_plugins - 1; ++j) {
        g_component_manager.loaded_plugins[j] = g_component_manager.loaded_plugins[j + 1];
      }
      g_component_manager.num_plugins--;
      
      printf("✓ Plugin unloaded: %s\n", plugin_name);
      return MODYN_SUCCESS;
    }
  }
  
  return MODYN_ERROR_INVALID_ARGUMENT; // 插件未找到
}

// 重新加载插件
static modyn_status_e plugin_loader_reload_plugin(const char *plugin_name) {
  if (!plugin_name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 查找插件
  modyn_plugin_info_t *plugin = NULL;
  for (size_t i = 0; i < g_component_manager.num_plugins; ++i) {
    if (strcmp(g_component_manager.loaded_plugins[i].name, plugin_name) == 0) {
      plugin = &g_component_manager.loaded_plugins[i];
      break;
    }
  }
  
  if (!plugin) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 保存文件路径
  char plugin_path[256];
  strncpy(plugin_path, plugin->file_path, sizeof(plugin_path)-1);
  
  // 卸载插件
  modyn_status_e status = plugin_loader_unload_plugin(plugin_name);
  if (status != MODYN_SUCCESS) return status;
  
  // 重新加载插件
  return plugin_loader_load_plugin(plugin_path, plugin);
}

// 列出插件
static modyn_status_e plugin_loader_list_plugins(modyn_plugin_info_t *plugins, 
                                                size_t max_plugins, size_t *count) {
  if (!plugins || !count) return MODYN_ERROR_INVALID_ARGUMENT;
  
  size_t copy_count = (g_component_manager.num_plugins < max_plugins) ? 
                      g_component_manager.num_plugins : max_plugins;
  
  for (size_t i = 0; i < copy_count; ++i) {
    plugins[i] = g_component_manager.loaded_plugins[i];
  }
  
  *count = copy_count;
  return MODYN_SUCCESS;
}

// 获取插件信息
static modyn_status_e plugin_loader_get_plugin_info(const char *plugin_name, 
                                                   modyn_plugin_info_t *info) {
  if (!plugin_name || !info) return MODYN_ERROR_INVALID_ARGUMENT;
  
  for (size_t i = 0; i < g_component_manager.num_plugins; ++i) {
    if (strcmp(g_component_manager.loaded_plugins[i].name, plugin_name) == 0) {
      *info = g_component_manager.loaded_plugins[i];
      return MODYN_SUCCESS;
    }
  }
  
  return MODYN_ERROR_INVALID_ARGUMENT; // 插件未找到
}

// 验证插件
static modyn_status_e plugin_loader_validate_plugin(const char *plugin_path, int *is_valid) {
  if (!plugin_path || !is_valid) return MODYN_ERROR_INVALID_ARGUMENT;
  
  *is_valid = (validate_plugin_file(plugin_path) == MODYN_SUCCESS);
  return MODYN_SUCCESS;
}

// 检查兼容性
static modyn_status_e plugin_loader_check_compatibility(const char *plugin_path, int *is_compatible) {
  if (!plugin_path || !is_compatible) return MODYN_ERROR_INVALID_ARGUMENT;
  
  *is_compatible = (check_plugin_compatibility(plugin_path) == MODYN_SUCCESS);
  return MODYN_SUCCESS;
}

// ======================== 内部辅助函数 ======================== //

// 验证插件文件
static modyn_status_e validate_plugin_file(const char *plugin_path) {
  if (!plugin_path) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 检查文件是否存在
  struct stat st;
  if (stat(plugin_path, &st) != 0) {
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
  
  // 检查是否为普通文件
  if (!S_ISREG(st.st_mode)) {
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
  
  // 检查文件扩展名
  const char *ext = strrchr(plugin_path, '.');
  if (!ext || strcmp(ext, ".so") != 0) {
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
  
  // 检查文件权限
  if (access(plugin_path, R_OK) != 0) {
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
  
  return MODYN_SUCCESS;
}

// 检查插件兼容性
static modyn_status_e check_plugin_compatibility(const char *plugin_path) {
  if (!plugin_path) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 尝试加载插件以检查兼容性
  void *handle = dlopen(plugin_path, RTLD_LAZY);
  if (!handle) {
    return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
  }
  
  // 检查必要的符号是否存在
  void *init_symbol = dlsym(handle, "modyn_plugin_init");
  void *version_symbol = dlsym(handle, "modyn_plugin_version");
  
  dlclose(handle);
  
  if (!init_symbol || !version_symbol) {
    return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
  }
  
  return MODYN_SUCCESS;
}

// 加载插件符号
static modyn_status_e load_plugin_symbols(void *handle, modyn_plugin_info_t *info) {
  if (!handle || !info) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 清除之前的错误
  dlerror();
  
  // 获取插件初始化函数
  typedef modyn_status_e (*plugin_init_fn)(modyn_plugin_info_t*);
  plugin_init_fn init_func = (plugin_init_fn)dlsym(handle, "modyn_plugin_init");
  if (!init_func) {
    printf("Failed to find modyn_plugin_init symbol: %s\n", dlerror());
    return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
  }
  
  // 调用插件初始化函数
  modyn_status_e status = init_func(info);
  if (status != MODYN_SUCCESS) {
    printf("Plugin initialization failed: %d\n", status);
    return status;
  }
  
  return MODYN_SUCCESS;
}

// 扫描目录中的插件
static modyn_status_e scan_directory_for_plugins(const char *dir_path) {
  if (!dir_path) return MODYN_ERROR_INVALID_ARGUMENT;
  
  DIR *dir = opendir(dir_path);
  if (!dir) return MODYN_ERROR_INVALID_ARGUMENT;
  
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    // 跳过 . 和 ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    
    // 检查是否为.so文件
    if (strstr(entry->d_name, ".so") != NULL) {
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
      
      // 尝试加载插件
      modyn_status_e status = modyn_load_plugin(full_path);
      if (status != MODYN_SUCCESS) {
        printf("Warning: Failed to load plugin %s: %d\n", full_path, status);
      }
    }
  }
  
  closedir(dir);
  return MODYN_SUCCESS;
}

// 清理插件信息
static void cleanup_plugin_info(modyn_plugin_info_t *info) {
  if (!info) return;
  
  memset(info->name, 0, sizeof(info->name));
  memset(info->version, 0, sizeof(info->version));
  memset(info->description, 0, sizeof(info->description));
  memset(info->author, 0, sizeof(info->author));
  memset(info->license, 0, sizeof(info->license));
  memset(info->file_path, 0, sizeof(info->file_path));
  info->handle = NULL;
  info->source = MODYN_COMPONENT_SOURCE_PLUGIN;
  info->load_time = 0;
}

// ======================== 公共API实现 ======================== //

// 初始化组件管理器
modyn_status_e modyn_component_manager_init(const void *config) {
  if (g_initialized) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 初始化组件注册表
  memset(&g_component_manager, 0, sizeof(g_component_manager));
  
  // 初始化插件加载器
  g_component_manager.plugin_loader.load_plugin = plugin_loader_load_plugin;
  g_component_manager.plugin_loader.unload_plugin = plugin_loader_unload_plugin;
  g_component_manager.plugin_loader.reload_plugin = plugin_loader_reload_plugin;
  g_component_manager.plugin_loader.list_plugins = plugin_loader_list_plugins;
  g_component_manager.plugin_loader.get_plugin_info = plugin_loader_get_plugin_info;
  g_component_manager.plugin_loader.validate_plugin = plugin_loader_validate_plugin;
  g_component_manager.plugin_loader.check_compatibility = plugin_loader_check_compatibility;
  
  // 设置默认插件搜索路径
  g_component_manager.plugin_search_paths[0][0] = '\0'; // 当前目录
  strcpy(g_component_manager.plugin_search_paths[1], "./plugins");
  strcpy(g_component_manager.plugin_search_paths[2], "/usr/local/lib/modyn/plugins");
  strcpy(g_component_manager.plugin_search_paths[3], "/usr/lib/modyn/plugins");
  g_component_manager.num_search_paths = 4;
  
  // 启用自动发现
  g_component_manager.auto_discovery_enabled = 1;
  g_component_manager.hot_reload_enabled = 0;
  
  // 分配插件数组
  g_component_manager.max_plugins = 32;
  g_component_manager.loaded_plugins = (modyn_plugin_info_t*)calloc(
    g_component_manager.max_plugins, sizeof(modyn_plugin_info_t));
  
  if (!g_component_manager.loaded_plugins) {
    return MODYN_ERROR_MEMORY_ALLOCATION;
  }
  
  g_initialized = 1;
  printf("✓ Component manager initialized\n");
  
  // 手动注册内建组件（因为静态库中的__attribute__((constructor))不会自动执行）
  printf("--- 手动注册内建组件 ---\n");
  
  return MODYN_SUCCESS;
}

// 关闭组件管理器
modyn_status_e modyn_component_manager_shutdown(void) {
  if (!g_initialized) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 卸载所有插件
  for (size_t i = 0; i < g_component_manager.num_plugins; ++i) {
    if (g_component_manager.loaded_plugins[i].handle) {
      dlclose(g_component_manager.loaded_plugins[i].handle);
    }
  }
  
  // 清理插件数组
  if (g_component_manager.loaded_plugins) {
    free(g_component_manager.loaded_plugins);
    g_component_manager.loaded_plugins = NULL;
  }
  
  // 清理组件注册表
  for (int i = 0; i < MODYN_COMPONENT_COUNT; ++i) {
    modyn_component_registry_t *registry = g_component_manager.registries[i];
    while (registry) {
      modyn_component_registry_t *next = registry->next;
      free(registry);
      registry = next;
    }
    g_component_manager.registries[i] = NULL;
    g_component_manager.component_counts[i] = 0;
  }
  
  g_initialized = 0;
  printf("✓ Component manager shutdown\n");
  return MODYN_SUCCESS;
}

// 注册组件
modyn_status_e modyn_register_component(modyn_component_type_e type,
                                       const char *name,
                                       modyn_component_interface_t *interface,
                                       modyn_component_source_e source) {
  if (!g_initialized || !name || !interface) return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (type < 0 || type >= MODYN_COMPONENT_COUNT) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 检查组件是否已存在
  modyn_component_registry_t *existing = g_component_manager.registries[type];
  while (existing) {
    if (strcmp(existing->type_name, name) == 0) {
      printf("Warning: Component %s already registered\n", name);
      return MODYN_ERROR_INVALID_ARGUMENT;
    }
    existing = existing->next;
  }
  
  // 创建新的注册项
  modyn_component_registry_t *registry = (modyn_component_registry_t*)malloc(
    sizeof(modyn_component_registry_t));
  if (!registry) return MODYN_ERROR_MEMORY_ALLOCATION;
  
  registry->type = type;
  registry->type_name = name;
  registry->interface = interface;
  registry->next = g_component_manager.registries[type];
  
  // 添加到注册表头部
  g_component_manager.registries[type] = registry;
  g_component_manager.component_counts[type]++;
  
  printf("✓ Component registered: %s (type: %d, source: %d)\n", name, type, source);
  return MODYN_SUCCESS;
}

// 注销组件
modyn_status_e modyn_unregister_component(modyn_component_type_e type, const char *name) {
  if (!g_initialized || !name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (type < 0 || type >= MODYN_COMPONENT_COUNT) return MODYN_ERROR_INVALID_ARGUMENT;
  
  modyn_component_registry_t *prev = NULL;
  modyn_component_registry_t *current = g_component_manager.registries[type];
  
  while (current) {
    if (strcmp(current->type_name, name) == 0) {
      // 从链表中移除
      if (prev) {
        prev->next = current->next;
      } else {
        g_component_manager.registries[type] = current->next;
      }
      
      // 清理
      free(current);
      g_component_manager.component_counts[type]--;
      
      printf("✓ Component unregistered: %s\n", name);
      return MODYN_SUCCESS;
    }
    
    prev = current;
    current = current->next;
  }
  
  return MODYN_ERROR_INVALID_ARGUMENT; // 组件未找到
}

// 查找组件
modyn_component_interface_t* modyn_find_component(modyn_component_type_e type, const char *name) {
  if (!g_initialized || !name) return NULL;
  
  if (type < 0 || type >= MODYN_COMPONENT_COUNT) return NULL;
  
  modyn_component_registry_t *registry = g_component_manager.registries[type];
  while (registry) {
    if (strcmp(registry->type_name, name) == 0) {
      return registry->interface;
    }
    registry = registry->next;
  }
  
  return NULL; // 组件未找到
}

// 获取组件列表
modyn_status_e modyn_get_components(modyn_component_type_e type,
                                   modyn_component_interface_t **components,
                                   size_t max_components,
                                   size_t *count) {
  if (!g_initialized || !components || !count) return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (type < 0 || type >= MODYN_COMPONENT_COUNT) return MODYN_ERROR_INVALID_ARGUMENT;
  
  size_t found = 0;
  modyn_component_registry_t *registry = g_component_manager.registries[type];
  
  while (registry && found < max_components) {
    components[found++] = registry->interface;
    registry = registry->next;
  }
  
  *count = found;
  return MODYN_SUCCESS;
}

// 加载插件
modyn_status_e modyn_load_plugin(const char *plugin_path) {
  if (!g_initialized || !plugin_path) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 检查插件数量限制
  if (g_component_manager.num_plugins >= g_component_manager.max_plugins) {
    return MODYN_ERROR_MEMORY_ALLOCATION;
  }
  
  // 使用插件加载器加载插件
  modyn_plugin_info_t *info = &g_component_manager.loaded_plugins[g_component_manager.num_plugins];
  modyn_status_e status = g_component_manager.plugin_loader.load_plugin(plugin_path, info);
  
  if (status == MODYN_SUCCESS) {
    g_component_manager.num_plugins++;
  }
  
  return status;
}

// 卸载插件
modyn_status_e modyn_unload_plugin(const char *plugin_name) {
  if (!g_initialized || !plugin_name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  return g_component_manager.plugin_loader.unload_plugin(plugin_name);
}

// 重新加载插件
modyn_status_e modyn_reload_plugin(const char *plugin_name) {
  if (!g_initialized || !plugin_name) return MODYN_ERROR_INVALID_ARGUMENT;
  
  return g_component_manager.plugin_loader.reload_plugin(plugin_name);
}

// 获取已加载的插件列表
modyn_status_e modyn_list_loaded_plugins(modyn_plugin_info_t *plugins,
                                        size_t max_plugins,
                                        size_t *count) {
  if (!g_initialized || !plugins || !count) return MODYN_ERROR_INVALID_ARGUMENT;
  
  return g_component_manager.plugin_loader.list_plugins(plugins, max_plugins, count);
}

// 设置插件搜索路径
modyn_status_e modyn_set_plugin_search_paths(const char **paths, size_t num_paths) {
  if (!g_initialized || !paths || num_paths > 8) return MODYN_ERROR_INVALID_ARGUMENT;
  
  g_component_manager.num_search_paths = num_paths;
  for (size_t i = 0; i < num_paths; ++i) {
    strncpy(g_component_manager.plugin_search_paths[i], paths[i], 255);
    g_component_manager.plugin_search_paths[i][255] = '\0';
  }
  
  return MODYN_SUCCESS;
}

// 启用/禁用插件自动发现
modyn_status_e modyn_set_plugin_auto_discovery(int enabled) {
  if (!g_initialized) return MODYN_ERROR_INVALID_ARGUMENT;
  
  g_component_manager.auto_discovery_enabled = enabled ? 1 : 0;
  return MODYN_SUCCESS;
}

// 扫描并加载插件目录中的所有插件
modyn_status_e modyn_scan_and_load_plugins(void) {
  if (!g_initialized) return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (!g_component_manager.auto_discovery_enabled) {
    return MODYN_SUCCESS;
  }
  
  printf("Scanning for plugins...\n");
  
  for (size_t i = 0; i < g_component_manager.num_search_paths; ++i) {
    if (g_component_manager.plugin_search_paths[i][0] != '\0') {
      modyn_status_e status = scan_directory_for_plugins(g_component_manager.plugin_search_paths[i]);
      if (status != MODYN_SUCCESS) {
        printf("Warning: Failed to scan directory %s\n", g_component_manager.plugin_search_paths[i]);
      }
    }
  }
  
  printf("Plugin scanning completed. Loaded %zu plugins.\n", g_component_manager.num_plugins);
  return MODYN_SUCCESS;
}

// 获取组件管理器统计信息
modyn_status_e modyn_get_component_manager_stats(size_t *total_components,
                                                size_t *builtin_components,
                                                size_t *plugin_components,
                                                size_t *loaded_plugins) {
  if (!g_initialized) return MODYN_ERROR_INVALID_ARGUMENT;
  
  if (total_components) {
    *total_components = 0;
    for (int i = 0; i < MODYN_COMPONENT_COUNT; ++i) {
      *total_components += g_component_manager.component_counts[i];
    }
  }
  
  if (builtin_components) {
    *builtin_components = 0; // 这里需要根据组件来源统计
  }
  
  if (plugin_components) {
    *plugin_components = 0; // 这里需要根据组件来源统计
  }
  
  if (loaded_plugins) {
    *loaded_plugins = g_component_manager.num_plugins;
  }
  
  return MODYN_SUCCESS;
}

// ======================== 格式化组件信息打印 ======================== //

/**
 * @brief 格式化打印所有已注册组件信息（类似FFmpeg格式）
 */
modyn_status_e modyn_print_registered_components(const char *output_format, int show_details) {
  if (!g_initialized) return MODYN_ERROR_INVALID_ARGUMENT;
  
  const char *format = output_format ? output_format : "text";
  
  if (strcmp(format, "text") == 0) {
    return print_components_text(show_details);
  } else if (strcmp(format, "json") == 0) {
    return print_components_json(show_details);
  } else if (strcmp(format, "xml") == 0) {
    return print_components_xml(show_details);
  } else if (strcmp(format, "csv") == 0) {
    return print_components_csv(show_details);
  } else {
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
}

/**
 * @brief 获取组件信息的JSON格式字符串
 */
modyn_status_e modyn_get_components_json(char *json_buffer, size_t buffer_size) {
  if (!g_initialized || !json_buffer || buffer_size == 0) {
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
  
  return generate_components_json(json_buffer, buffer_size);
}

/**
 * @brief 获取组件信息的XML格式字符串
 */
modyn_status_e modyn_get_components_xml(char *xml_buffer, size_t buffer_size) {
  if (!g_initialized || !xml_buffer || buffer_size == 0) {
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
  
  return generate_components_xml(xml_buffer, buffer_size);
}

// ======================== 内部格式化函数 ======================== //

/**
 * @brief 文本格式打印组件信息（类似FFmpeg格式）
 */
static modyn_status_e print_components_text(int show_details) {
  printf("\nModyn Components Information\n");
  printf("============================\n\n");
  
  // 打印统计信息
  size_t total_components = 0;
  size_t builtin_components = 0;
  size_t plugin_components = 0;
  size_t loaded_plugins = 0;
  
  modyn_get_component_manager_stats(&total_components, &builtin_components, 
                                   &plugin_components, &loaded_plugins);
  
  printf("Summary:\n");
  printf("  Total Components: %zu\n", total_components);
  printf("  Built-in Components: %zu\n", builtin_components);
  printf("  Plugin Components: %zu\n", plugin_components);
  printf("  Loaded Plugins: %zu\n\n", loaded_plugins);
  
  if (total_components == 0) {
    printf("No components registered.\n\n");
    return MODYN_SUCCESS;
  }
  
  // 按类型分组打印组件
  for (int type = 0; type < MODYN_COMPONENT_COUNT; ++type) {
    modyn_component_registry_t *registry = g_component_manager.registries[type];
    if (!registry) continue;
    
    size_t count = g_component_manager.component_counts[type];
    if (count == 0) continue;
    
    // 获取类型名称
    const char *type_name = get_component_type_name(type);
    printf("%s Components (%zu):\n", type_name, count);
    printf("  %-20s %-15s %-10s %-15s %s\n", 
           "Name", "Version", "Source", "Status", "Capabilities");
    printf("  %-20s %-15s %-10s %-15s %s\n", 
           "----", "-------", "------", "------", "------------");
    
    modyn_component_registry_t *current = registry;
    while (current) {
      modyn_component_interface_t *comp = current->interface;
      if (comp) {
        // 获取组件状态
        modyn_component_status_e status = MODYN_COMPONENT_LOADED;
        if (comp->get_status) {
          status = comp->get_status(comp->private_data);
        }
        
        // 获取能力描述
        const char *capabilities = "N/A";
        if (comp->get_capabilities) {
          capabilities = comp->get_capabilities(comp->private_data);
        }
        
        // 获取来源名称
        const char *source_name = get_component_source_name(comp->source);
        
        // 获取状态名称
        const char *status_name = get_component_status_name(status);
        
        printf("  %-20s %-15s %-10s %-15s %s\n",
               comp->name, comp->version, source_name, status_name, capabilities);
        
        if (show_details) {
          printf("    Details:\n");
          printf("      Type: %s\n", type_name);
          printf("      Source: %s\n", source_name);
          printf("      Status: %s\n", status_name);
          printf("      Capabilities: %s\n", capabilities);
          
          // 测试功能支持
          if (comp->supports_feature) {
            printf("      Features:\n");
            test_component_features(comp);
          }
          printf("\n");
        }
      }
      current = current->next;
    }
    printf("\n");
  }
  
  return MODYN_SUCCESS;
}

/**
 * @brief JSON格式打印组件信息
 */
static modyn_status_e print_components_json(int show_details) {
  char json_buffer[8192];
  modyn_status_e status = generate_components_json(json_buffer, sizeof(json_buffer));
  if (status == MODYN_SUCCESS) {
    printf("%s\n", json_buffer);
  }
  return status;
}

/**
 * @brief XML格式打印组件信息
 */
static modyn_status_e print_components_xml(int show_details) {
  char xml_buffer[8192];
  modyn_status_e status = generate_components_xml(xml_buffer, sizeof(xml_buffer));
  if (status == MODYN_SUCCESS) {
    printf("%s\n", xml_buffer);
  }
  return status;
}

/**
 * @brief CSV格式打印组件信息
 */
static modyn_status_e print_components_csv(int show_details) {
  printf("Name,Version,Type,Source,Status,Capabilities\n");
  
  for (int type = 0; type < MODYN_COMPONENT_COUNT; ++type) {
    modyn_component_registry_t *registry = g_component_manager.registries[type];
    if (!registry) continue;
    
    const char *type_name = get_component_type_name(type);
    modyn_component_registry_t *current = registry;
    
    while (current) {
      modyn_component_interface_t *comp = current->interface;
      if (comp) {
        modyn_component_status_e status = MODYN_COMPONENT_LOADED;
        if (comp->get_status) {
          status = comp->get_status(comp->private_data);
        }
        
        const char *capabilities = "N/A";
        if (comp->get_capabilities) {
          capabilities = comp->get_capabilities(comp->private_data);
        }
        
        const char *source_name = get_component_source_name(comp->source);
        const char *status_name = get_component_status_name(status);
        
        // CSV格式：转义逗号和引号
        printf("\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
               escape_csv_field(comp->name),
               escape_csv_field(comp->version),
               escape_csv_field(type_name),
               escape_csv_field(source_name),
               escape_csv_field(status_name),
               escape_csv_field(capabilities));
      }
      current = current->next;
    }
  }
  
  return MODYN_SUCCESS;
}

/**
 * @brief 生成JSON格式的组件信息
 */
static modyn_status_e generate_components_json(char *json_buffer, size_t buffer_size) {
  if (!json_buffer || buffer_size == 0) return MODYN_ERROR_INVALID_ARGUMENT;
  
  int offset = 0;
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "{\n  \"modyn_components\": {\n");
  
  // 统计信息
  size_t total_components = 0;
  size_t builtin_components = 0;
  size_t plugin_components = 0;
  size_t loaded_plugins = 0;
  
  modyn_get_component_manager_stats(&total_components, &builtin_components, 
                                   &plugin_components, &loaded_plugins);
  
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "    \"summary\": {\n");
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "      \"total_components\": %zu,\n", total_components);
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "      \"builtin_components\": %zu,\n", builtin_components);
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "      \"plugin_components\": %zu,\n", plugin_components);
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "      \"loaded_plugins\": %zu\n", loaded_plugins);
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "    },\n");
  
  // 组件信息
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "    \"components\": [\n");
  
  int component_count = 0;
  for (int type = 0; type < MODYN_COMPONENT_COUNT; ++type) {
    modyn_component_registry_t *registry = g_component_manager.registries[type];
    if (!registry) continue;
    
    const char *type_name = get_component_type_name(type);
    modyn_component_registry_t *current = registry;
    
    while (current) {
      modyn_component_interface_t *comp = current->interface;
      if (comp) {
        if (component_count > 0) {
          offset += snprintf(json_buffer + offset, buffer_size - offset, ",\n");
        }
        
        modyn_component_status_e status = MODYN_COMPONENT_LOADED;
        if (comp->get_status) {
          status = comp->get_status(comp->private_data);
        }
        
        const char *capabilities = "N/A";
        if (comp->get_capabilities) {
          capabilities = comp->get_capabilities(comp->private_data);
        }
        
        const char *source_name = get_component_source_name(comp->source);
        const char *status_name = get_component_status_name(status);
        
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "      {\n");
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "        \"name\": \"%s\",\n", comp->name);
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "        \"version\": \"%s\",\n", comp->version);
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "        \"type\": \"%s\",\n", type_name);
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "        \"source\": \"%s\",\n", source_name);
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "        \"status\": \"%s\",\n", status_name);
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "        \"capabilities\": \"%s\"\n", capabilities);
        offset += snprintf(json_buffer + offset, buffer_size - offset,
                           "      }");
        
        component_count++;
      }
      current = current->next;
    }
  }
  
  offset += snprintf(json_buffer + offset, buffer_size - offset,
                     "\n    ]\n  }\n}\n");
  
  return MODYN_SUCCESS;
}

/**
 * @brief 生成XML格式的组件信息
 */
static modyn_status_e generate_components_xml(char *xml_buffer, size_t buffer_size) {
  if (!xml_buffer || buffer_size == 0) return MODYN_ERROR_INVALID_ARGUMENT;
  
  int offset = 0;
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "<modyn_components>\n");
  
  // 统计信息
  size_t total_components = 0;
  size_t builtin_components = 0;
  size_t plugin_components = 0;
  size_t loaded_plugins = 0;
  
  modyn_get_component_manager_stats(&total_components, &builtin_components, 
                                   &plugin_components, &loaded_plugins);
  
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "  <summary>\n");
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "    <total_components>%zu</total_components>\n", total_components);
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "    <builtin_components>%zu</builtin_components>\n", builtin_components);
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "    <plugin_components>%zu</plugin_components>\n", plugin_components);
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "    <loaded_components>%zu</loaded_components>\n", loaded_plugins);
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "  </summary>\n");
  
  // 组件信息
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "  <components>\n");
  
  for (int type = 0; type < MODYN_COMPONENT_COUNT; ++type) {
    modyn_component_registry_t *registry = g_component_manager.registries[type];
    if (!registry) continue;
    
    const char *type_name = get_component_type_name(type);
    modyn_component_registry_t *current = registry;
    
    while (current) {
      modyn_component_interface_t *comp = current->interface;
      if (comp) {
        modyn_component_status_e status = MODYN_COMPONENT_LOADED;
        if (comp->get_status) {
          status = comp->get_status(comp->private_data);
        }
        
        const char *capabilities = "N/A";
        if (comp->get_capabilities) {
          capabilities = comp->get_capabilities(comp->private_data);
        }
        
        const char *source_name = get_component_source_name(comp->source);
        const char *status_name = get_component_status_name(status);
        
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "    <component>\n");
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "      <name>%s</name>\n", comp->name);
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "      <version>%s</version>\n", comp->version);
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "      <type>%s</type>\n", type_name);
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "      <source>%s</source>\n", source_name);
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "      <status>%s</status>\n", status_name);
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "      <capabilities>%s</capabilities>\n", capabilities);
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
                           "    </component>\n");
      }
      current = current->next;
    }
  }
  
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "  </components>\n");
  offset += snprintf(xml_buffer + offset, buffer_size - offset,
                     "</modyn_components>\n");
  
  return MODYN_SUCCESS;
}

// ======================== 辅助函数 ======================== //

/**
 * @brief 获取组件类型名称
 */
static const char* get_component_type_name(int type) {
  switch (type) {
    case MODYN_COMPONENT_DEVICE: return "Device";
    case MODYN_COMPONENT_MEMORY_POOL: return "MemoryPool";
    case MODYN_COMPONENT_MODEL_LOADER: return "ModelLoader";
    case MODYN_COMPONENT_PIPELINE_NODE: return "PipelineNode";
    default: return "Unknown";
  }
}

/**
 * @brief 获取组件来源名称
 */
static const char* get_component_source_name(modyn_component_source_e source) {
  switch (source) {
    case MODYN_COMPONENT_SOURCE_BUILTIN: return "Built-in";
    case MODYN_COMPONENT_SOURCE_PLUGIN: return "Plugin";
    case MODYN_COMPONENT_SOURCE_DYNAMIC: return "Dynamic";
    default: return "Unknown";
  }
}

/**
 * @brief 获取组件状态名称
 */
static const char* get_component_status_name(modyn_component_status_e status) {
  switch (status) {
    case MODYN_COMPONENT_LOADED: return "Loaded";
    case MODYN_COMPONENT_ACTIVE: return "Active";
    case MODYN_COMPONENT_INACTIVE: return "Inactive";
    case MODYN_COMPONENT_ERROR: return "Error";
    case MODYN_COMPONENT_UNLOADED: return "Unloaded";
    default: return "Unknown";
  }
}

/**
 * @brief 测试组件功能支持
 */
static void test_component_features(modyn_component_interface_t *comp) {
  if (!comp->supports_feature) return;
  
  const char *test_features[] = {
    "basic_inference", "gpu_inference", "tensor_ops", 
    "memory_management", "cuda_support", "feature_x", "feature_z"
  };
  
  for (int i = 0; i < sizeof(test_features)/sizeof(test_features[0]); ++i) {
    int supported = comp->supports_feature(comp->private_data, test_features[i]);
    if (supported) {
      printf("        ✓ %s\n", test_features[i]);
    }
  }
}

/**
 * @brief 转义CSV字段
 */
static const char* escape_csv_field(const char *field) {
  if (!field) return "";
  
  // 如果字段包含逗号、引号或换行符，需要转义
  if (strchr(field, ',') || strchr(field, '"') || strchr(field, '\n')) {
    // 这里简化处理，实际应该分配内存并转义
    return field;
  }
  
  return field;
}
