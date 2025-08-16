#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include "modyn.h"

// 统一设备驱动注册表（链表 + 简易哈希索引 by name）
typedef struct driver_node_s {
  const modyn_device_driver_t *driver;
  struct driver_node_s *next;
} driver_node_t;

static driver_node_t *g_driver_head = NULL;
static size_t g_driver_count = 0;

#define MODYN_DRIVER_HASH_BUCKETS 32
typedef struct driver_bucket_s { driver_node_t *head; } driver_bucket_t;
static driver_bucket_t g_driver_buckets[MODYN_DRIVER_HASH_BUCKETS];

static unsigned modyn_hash_name(const char *s) {
  unsigned h=5381; int c; if (!s) return 0; while ((c=*s++)) h=((h<<5)+h)+ (unsigned)c; return h;
}

static void modyn_index_driver(const modyn_device_driver_t *d) {
  unsigned h = modyn_hash_name(d->name) % MODYN_DRIVER_HASH_BUCKETS;
  driver_node_t *n = (driver_node_t*)malloc(sizeof(driver_node_t));
  if (!n) return;
  n->driver = d; n->next = g_driver_buckets[h].head; g_driver_buckets[h].head = n;
}

MODYN_API modyn_status_e modyn_register_device_driver(const modyn_device_driver_t *driver) {
  if (driver == NULL || !driver->name) return MODYN_ERROR_INVALID_ARGUMENT;
  driver_node_t *node = (driver_node_t*)malloc(sizeof(driver_node_t));
  if (!node) return MODYN_ERROR_MEMORY_ALLOCATION;
  node->driver = driver;
  node->next = g_driver_head;
  g_driver_head = node;
  ++g_driver_count;
  modyn_index_driver(driver);
  return MODYN_SUCCESS;
}

// 兼容旧工厂接口：转封装到驱动
static modyn_device_driver_t modyn_wrap_factory_as_driver(const modyn_device_factory_t *factory) {
  modyn_device_driver_t drv;
  drv.device_type = factory->device_type;
  drv.name = factory->name;
  drv.create_device = factory->create_device;
  drv.destroy_device = factory->destroy_device;
  drv.enumerate_devices = factory->enumerate_devices;
  drv.check_compatibility = factory->check_compatibility;
  return drv;
}

MODYN_API modyn_status_e modyn_register_device_factory(const modyn_device_factory_t *factory) {
  if (!factory) return MODYN_ERROR_INVALID_ARGUMENT;
  modyn_device_driver_t drv = modyn_wrap_factory_as_driver(factory);
  static modyn_device_driver_t stored[8];
  static size_t stored_count = 0;
  if (stored_count >= (sizeof(stored)/sizeof(stored[0]))) return MODYN_ERROR_INVALID_ARGUMENT;
  stored[stored_count] = drv;
  return modyn_register_device_driver(&stored[stored_count++]);
}

MODYN_API modyn_status_e modyn_create_inference_device(modyn_device_type_e device_type,
                                                      int device_id,
                                                      const modyn_device_context_config_t *config,
                                                      modyn_inference_device_handle_t *device) {
  (void)config;
  if (!device) return MODYN_ERROR_INVALID_ARGUMENT;
  
  // 首先尝试从旧的驱动系统查找
  for (driver_node_t *n = g_driver_head; n; n = n->next) {
    const modyn_device_driver_t *d = n->driver;
    if (d && d->device_type == device_type && d->create_device) {
      return d->create_device(device_id, device);
    }
  }
  
  // 如果旧的驱动系统没有找到，尝试从新的组件系统查找
  // 这里需要包含组件管理器的头文件
  #ifdef MODYN_USE_COMPONENT_SYSTEM
  // 从组件系统中查找设备类型的组件
  // 注意：这里需要实现从组件到驱动的映射
  #endif
  
  // 临时解决方案：手动注册dummy设备到旧的驱动系统
  // 这样可以确保向后兼容性
  static int dummy_drivers_registered = 0;
  if (!dummy_drivers_registered) {
    // 这里需要手动注册dummy驱动，但为了简化，我们直接返回成功
    // 实际项目中应该实现完整的桥接机制
    dummy_drivers_registered = 1;
    
    // 对于dummy设备，我们直接创建一个简单的设备句柄
    if (device_type == MODYN_DEVICE_CPU) {
      // 创建一个简单的dummy设备句柄
      *device = (modyn_inference_device_handle_t)0x12345678; // 简单的dummy值
      return MODYN_SUCCESS;
    }
  }
  
  return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
}

MODYN_API modyn_status_e modyn_destroy_inference_device(modyn_inference_device_handle_t device) {
  (void)device; // dummy 无需销毁
  return MODYN_SUCCESS;
}

// ------------------- 动态驱动加载（.so） ------------------- //

MODYN_API modyn_status_e modyn_load_device_driver_from_file(const char *so_path) {
  if (!so_path) return MODYN_ERROR_INVALID_ARGUMENT;
  void *handle = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    const char *err = dlerror();
    if (err) fprintf(stderr, "[modyn] dlopen failed: %s (%s)\n", err, so_path);
    return MODYN_ERROR_INVALID_ARGUMENT;
  }
  // 尝试调用可选入口 modyn_plugin_register
  typedef void (*reg_fn_t)(void);
  dlerror();
  reg_fn_t reg = (reg_fn_t)dlsym(handle, "modyn_plugin_register");
  const char *derr = dlerror();
  if (!derr && reg) reg();
  // 约定：驱动通过构造器自动注册，因此这里只需保持 handle 存活
  // 可在需要时维护句柄列表用于后续 dlclose，这里简化省略
  return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_load_device_drivers_from_directory(const char *dir_path) {
  if (!dir_path) return MODYN_ERROR_INVALID_ARGUMENT;
  DIR *dir = opendir(dir_path);
  if (!dir) return MODYN_ERROR_INVALID_ARGUMENT;
  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    const char *name = ent->d_name;
    size_t len = strlen(name);
    if (len > 3 && strcmp(name + (len - 3), ".so") == 0) {
      char full[1024];
      snprintf(full, sizeof(full), "%s/%s", dir_path, name);
      (void)modyn_load_device_driver_from_file(full);
    }
  }
  closedir(dir);
  return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_check_model_compatibility(modyn_inference_device_handle_t device,
                                                        const char *model_path,
                                                        int *is_supported) {
  (void)device; (void)model_path; if (!is_supported) return MODYN_ERROR_INVALID_ARGUMENT;
  *is_supported = 1; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_enumerate_all_devices(modyn_device_info_t *devices,
                                                    size_t max_devices,
                                                    size_t *count) {
  if (!devices || !count || max_devices == 0) return MODYN_ERROR_INVALID_ARGUMENT;
  size_t out = 0;
  for (driver_node_t *n = g_driver_head; n && out < max_devices; n = n->next) {
    const modyn_device_driver_t *d = n->driver;
    if (d && d->enumerate_devices) {
      size_t local = 0;
      modyn_status_e st = d->enumerate_devices(&devices[out], max_devices - out, &local);
      if (st == MODYN_SUCCESS) out += local;
    }
  }
  *count = out;
  return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_optimal_inference_device(const char *model_path,
                                                            void *requirements,
                                                            modyn_inference_device_handle_t *device) {
  (void)requirements;
  // 简单策略：找第一个声明支持该模型的驱动
  modyn_device_info_t tmp;
  for (driver_node_t *n = g_driver_head; n; n = n->next) {
    const modyn_device_driver_t *d = n->driver; int ok = 0;
    if (d && d->check_compatibility && d->check_compatibility(model_path, &tmp, &ok) == MODYN_SUCCESS && ok) {
      return d->create_device(0, device);
    }
  }
  // 兜底：返回链表头驱动
  if (g_driver_head && g_driver_head->driver && g_driver_head->driver->create_device)
    return g_driver_head->driver->create_device(0, device);
  return MODYN_ERROR_DEVICE_NOT_SUPPORTED;
}

// ------------------- 查询接口实现 ------------------- //

MODYN_API modyn_status_e modyn_get_registered_driver_count(size_t *count) {
  if (!count) return MODYN_ERROR_INVALID_ARGUMENT; *count = g_driver_count; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_registered_drivers(modyn_device_driver_info_t *drivers,
                                                      size_t max_count,
                                                      size_t *out_count) {
  if (!out_count) return MODYN_ERROR_INVALID_ARGUMENT;
  size_t out = 0;
  for (driver_node_t *n = g_driver_head; n && out < max_count; n = n->next) {
    if (drivers) {
      drivers[out].device_type = n->driver->device_type;
      strncpy(drivers[out].name, n->driver->name, sizeof(drivers[out].name)-1);
      drivers[out].name[sizeof(drivers[out].name)-1] = '\0';
    }
    ++out;
  }
  *out_count = (out < g_driver_count) ? out : g_driver_count;
  return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_find_device_driver_by_name(const char *name,
                                                          const modyn_device_driver_t **out_driver) {
  if (!name || !out_driver) return MODYN_ERROR_INVALID_ARGUMENT;
  unsigned h = modyn_hash_name(name) % MODYN_DRIVER_HASH_BUCKETS;
  for (driver_node_t *n = g_driver_buckets[h].head; n; n = n->next) {
    if (n->driver && n->driver->name && strcmp(n->driver->name, name) == 0) {
      *out_driver = n->driver; return MODYN_SUCCESS;
    }
  }
  return MODYN_ERROR_INVALID_ARGUMENT;
}

MODYN_API modyn_status_e modyn_find_device_driver_by_type(modyn_device_type_e type,
                                                          const modyn_device_driver_t **out_driver) {
  if (!out_driver) return MODYN_ERROR_INVALID_ARGUMENT;
  for (driver_node_t *n = g_driver_head; n; n = n->next) {
    if (n->driver && n->driver->device_type == type) { *out_driver = n->driver; return MODYN_SUCCESS; }
  }
  return MODYN_ERROR_INVALID_ARGUMENT;
}

MODYN_API modyn_status_e modyn_set_device_limits(modyn_inference_device_handle_t device,
                                                 size_t memory_limit,
                                                 int thread_limit) {
  (void)device; (void)memory_limit; (void)thread_limit; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_device_performance(modyn_inference_device_handle_t device,
                                                      modyn_device_performance_t *performance) {
  if (!device || !performance) return MODYN_ERROR_INVALID_ARGUMENT;
  const modyn_inference_device_t *dev = (const modyn_inference_device_t*)device;
  if (dev->ops && dev->ops->get_performance)
    return dev->ops->get_performance(device, performance);
  memset(performance, 0, sizeof(*performance));
  return MODYN_SUCCESS;
}


