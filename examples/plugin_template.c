#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "component_manager.h"

// ======================== 插件信息 ======================== //

// 插件版本信息
const char* modyn_plugin_version(void) {
  return "1.0.0";
}

// 插件描述
const char* modyn_plugin_description(void) {
  return "Example plugin template for Modyn framework";
}

// 插件作者
const char* modyn_plugin_author(void) {
  return "Modyn Team";
}

// 插件许可证
const char* modyn_plugin_license(void) {
  return "MIT";
}

// ======================== 示例组件实现 ======================== //

// 示例推理设备组件
typedef struct example_device_s {
  char name[64];
  int device_id;
  int is_initialized;
  void *private_data;
} example_device_t;

// 注意：生命周期管理方法已移除，由组件自己负责
// 这些函数可以保留作为内部实现，但不再通过组件接口调用

// 获取设备状态
static modyn_component_status_e example_device_get_status(const void *component) {
  const example_device_t *device = (const example_device_t*)component;
  if (!device) return MODYN_COMPONENT_ERROR;
  
  return device->is_initialized ? MODYN_COMPONENT_ACTIVE : MODYN_COMPONENT_INACTIVE;
}

// 设备查询
static modyn_status_e example_device_query(const void *component, void *query_info) {
  const example_device_t *device = (const example_device_t*)component;
  if (!device) return MODYN_ERROR_INVALID_ARGUMENT;
  
  printf("  [Example Device] Query: %s (ID: %d, Status: %s)\n", 
         device->name, device->device_id, 
         device->is_initialized ? "Active" : "Inactive");
  return MODYN_SUCCESS;
}

// 检查设备能力
static int example_device_supports_feature(const void *component, const char *feature) {
  const example_device_t *device = (const example_device_t*)component;
  if (!device || !feature) return 0;
  
  // 示例：支持基本推理功能
  if (strcmp(feature, "basic_inference") == 0) return 1;
  if (strcmp(feature, "tensor_ops") == 0) return 1;
  
  return 0;
}

// 获取设备能力
static const char* example_device_get_capabilities(const void *component) {
  return "Basic inference, Tensor operations, Memory management";
}

// 示例内存池组件
typedef struct example_mempool_s {
  char name[64];
  size_t total_size;
  size_t used_size;
  int is_initialized;
} example_mempool_t;

// 注意：生命周期管理方法已移除，由组件自己负责
// 这些函数可以保留作为内部实现，但不再通过组件接口调用

// 内存池重置
static modyn_status_e example_mempool_reset(void *component) {
  example_mempool_t *mempool = (example_mempool_t*)component;
  if (!mempool) return MODYN_ERROR_INVALID_ARGUMENT;
  
  mempool->used_size = 0;
  printf("  [Example Memory Pool] Reset: %s\n", mempool->name);
  return MODYN_SUCCESS;
}

// 获取内存池状态
static modyn_component_status_e example_mempool_get_status(const void *component) {
  const example_mempool_t *mempool = (const example_mempool_t*)component;
  if (!mempool) return MODYN_COMPONENT_ERROR;
  
  return mempool->is_initialized ? MODYN_COMPONENT_ACTIVE : MODYN_COMPONENT_INACTIVE;
}

// 内存池查询
static modyn_status_e example_mempool_query(const void *component, void *query_info) {
  const example_mempool_t *mempool = (const example_mempool_t*)component;
  if (!mempool) return MODYN_ERROR_INVALID_ARGUMENT;
  
  printf("  [Example Memory Pool] Query: %s (Total: %zu, Used: %zu)\n", 
         mempool->name, mempool->total_size, mempool->used_size);
  return MODYN_SUCCESS;
}

// 检查内存池能力
static int example_mempool_supports_feature(const void *component, const char *feature) {
  const example_mempool_t *mempool = (const example_mempool_t*)component;
  if (!mempool || !feature) return 0;
  
  // 示例：支持基本内存管理功能
  if (strcmp(feature, "basic_allocation") == 0) return 1;
  if (strcmp(feature, "memory_tracking") == 0) return 1;
  
  return 0;
}

// 获取内存池能力
static const char* example_mempool_get_capabilities(const void *component) {
  return "Basic allocation, Memory tracking, Pool management";
}

// ======================== 组件接口定义 ======================== //

// 示例推理设备接口
static modyn_component_interface_t example_device_interface = {
  .name = "example_device",
  .version = "1.0.0",
  .type = MODYN_COMPONENT_DEVICE,
  .source = MODYN_COMPONENT_SOURCE_PLUGIN,
  .initialize = example_device_initialize,
  .shutdown = example_device_shutdown,
  .reset = example_device_reset,
  .get_status = example_device_get_status,
  .query = example_device_query,
  .supports_feature = example_device_supports_feature,
  .get_capabilities = example_device_get_capabilities,
  .private_data = NULL
};

// 示例内存池接口
static modyn_component_interface_t example_mempool_interface = {
  .name = "example_mempool",
  .version = "1.0.0",
  .type = MODYN_COMPONENT_MEMORY_POOL,
  .source = MODYN_COMPONENT_SOURCE_PLUGIN,
  .initialize = example_mempool_initialize,
  .shutdown = example_mempool_shutdown,
  .reset = example_mempool_reset,
  .get_status = example_mempool_get_status,
  .query = example_mempool_query,
  .supports_feature = example_mempool_supports_feature,
  .get_capabilities = example_mempool_get_capabilities,
  .private_data = NULL
};

// ======================== 插件初始化函数 ======================== //

// 插件初始化 - 这是插件必须实现的函数
modyn_status_e modyn_plugin_init(modyn_plugin_info_t *info) {
  if (!info) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 设置插件基本信息
  strncpy(info->name, "example_plugin", sizeof(info->name)-1);
  strncpy(info->version, modyn_plugin_version(), sizeof(info->version)-1);
  strncpy(info->description, modyn_plugin_description(), sizeof(info->description)-1);
  strncpy(info->author, modyn_plugin_author(), sizeof(info->author)-1);
  strncpy(info->license, modyn_plugin_license(), sizeof(info->license)-1);
  
  // 注册组件
  modyn_status_e status;
  
  // 注册示例推理设备
  status = modyn_register_component(MODYN_COMPONENT_DEVICE, 
                                   "example_device", 
                                   &example_device_interface, 
                                   MODYN_COMPONENT_SOURCE_PLUGIN);
  if (status != MODYN_SUCCESS) {
    printf("Failed to register example device: %d\n", status);
    return status;
  }
  
  // 注册示例内存池
  status = modyn_register_component(MODYN_COMPONENT_MEMORY_POOL, 
                                   "example_mempool", 
                                   &example_mempool_interface, 
                                   MODYN_COMPONENT_SOURCE_PLUGIN);
  if (status != MODYN_SUCCESS) {
    printf("Failed to register example memory pool: %d\n", status);
    return status;
  }
  
  printf("✓ Example plugin initialized successfully\n");
  printf("  - Registered device: example_device\n");
  printf("  - Registered memory pool: example_mempool\n");
  
  return MODYN_SUCCESS;
}

// 插件清理函数（可选）
modyn_status_e modyn_plugin_cleanup(void) {
  printf("Example plugin cleanup\n");
  
  // 注销组件
  modyn_unregister_component(MODYN_COMPONENT_DEVICE, "example_device");
  modyn_unregister_component(MODYN_COMPONENT_MEMORY_POOL, "example_mempool");
  
  return MODYN_SUCCESS;
}

// ======================== 使用说明 ======================== //

/*
 * 使用此模板创建插件的步骤：
 * 
 * 1. 复制此文件并重命名
 * 2. 修改插件信息（名称、版本、描述等）
 * 3. 实现你的组件逻辑
 * 4. 在 modyn_plugin_init 中注册你的组件
 * 5. 编译为共享库 (.so)
 * 6. 将 .so 文件放在插件目录中
 * 
 * 编译命令示例：
 * gcc -shared -fPIC -o libmodyn_example.so plugin_template.c -ldl
 * 
 * 插件目录：
 * - ./plugins/
 * - /usr/local/lib/modyn/plugins/
 * - /usr/lib/modyn/plugins/
 * 
 * 环境变量：
 * MODYN_PLUGIN_DIR - 自定义插件目录
 */
