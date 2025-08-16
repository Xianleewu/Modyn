#include <string.h>
#include <stdio.h>
#include <time.h>
#include "modyn.h"
#include "../../component_manager.h"

// 这是一个以 GPU 类型注册的“动态”示例驱动，编译为 .so 插件。

static modyn_status_e gpu_create(int device_id, modyn_inference_device_handle_t *device) {
  (void)device_id; if (!device) return MODYN_ERROR_INVALID_ARGUMENT;
  static modyn_inference_device_t s_dev;
  memset(&s_dev, 0, sizeof(s_dev));
  s_dev.info.type = MODYN_DEVICE_GPU;
  strncpy(s_dev.info.name, "Dummy GPU Device", sizeof(s_dev.info.name)-1);
  strncpy(s_dev.info.vendor, "Modyn", sizeof(s_dev.info.vendor)-1);
  strncpy(s_dev.info.driver_version, "0.1.0", sizeof(s_dev.info.driver_version)-1);
  s_dev.ops = NULL; // 示例插件不实现具体推理，仅用于展示可加载性
  *device = (modyn_inference_device_handle_t)&s_dev;
  return MODYN_SUCCESS;
}

static modyn_status_e gpu_destroy(modyn_inference_device_handle_t device) { (void)device; return MODYN_SUCCESS; }

static modyn_status_e gpu_enum(modyn_device_info_t *devices, size_t max, size_t *count) {
  if (!devices || !count || max == 0) return MODYN_ERROR_INVALID_ARGUMENT;
  devices[0].type = MODYN_DEVICE_GPU;
  strncpy(devices[0].name, "Dummy GPU Device", sizeof(devices[0].name)-1);
  strncpy(devices[0].vendor, "Modyn", sizeof(devices[0].vendor)-1);
  strncpy(devices[0].driver_version, "0.1.0", sizeof(devices[0].driver_version)-1);
  devices[0].device_id = 0; devices[0].numa_node = 0; *count = 1; return MODYN_SUCCESS;
}

static modyn_status_e gpu_check(const char *model_path, modyn_device_info_t *info, int *ok) {
  (void)model_path; (void)info; if (!ok) return MODYN_ERROR_INVALID_ARGUMENT; *ok = 1; return MODYN_SUCCESS;
}

// ======================== 新的组件自动注册系统 ======================== //

// 组件状态查询适配器
static modyn_component_status_e dummy_gpu_component_get_status(const void *component) {
  (void)component;
  // dummy GPU设备总是可用的
  return MODYN_COMPONENT_ACTIVE;
}

// 组件能力检查适配器
static int dummy_gpu_component_supports_feature(const void *component, const char *feature) {
  (void)component;
  if (!feature) return 0;
  
  // 检查支持的功能
  if (strcmp(feature, "gpu_inference") == 0) return 1;
  if (strcmp(feature, "tensor_ops") == 0) return 1;
  if (strcmp(feature, "memory_management") == 0) return 1;
  if (strcmp(feature, "cuda_support") == 0) return 1;
  
  return 0;
}

// 组件能力描述适配器
static const char* dummy_gpu_component_get_capabilities(const void *component) {
  (void)component;
  return "GPU inference, Tensor operations, Memory management, CUDA support, Dummy GPU simulation";
}

// 组件查询适配器
static modyn_status_e dummy_gpu_component_query_adapter(const void *component, void *query_info) {
  (void)component; (void)query_info;
  
  // 调用现有的查询逻辑，但需要适配新的接口
  printf("  [Dummy GPU Component] Query executed\n");
  return MODYN_SUCCESS;
}

// 定义dummy GPU设备组件接口 - 专注于管理功能，不重复生命周期管理
static modyn_component_interface_t dummy_gpu_device_interface = {
    .name = "dummy_gpu_device",
    .version = "0.1.0",
    .type = MODYN_COMPONENT_DEVICE,
    .source = MODYN_COMPONENT_SOURCE_PLUGIN,
    .query = dummy_gpu_component_query_adapter,  // 使用适配器函数
    .get_status = dummy_gpu_component_get_status,
    .supports_feature = dummy_gpu_component_supports_feature,
    .get_capabilities = dummy_gpu_component_get_capabilities,
    .private_data = NULL  // 稍后设置
};

static const modyn_device_driver_t g_dummy_gpu_driver = {
  .device_type = MODYN_DEVICE_GPU,
  .name = "dummy_gpu",
  .version = MODYN_COMPONENT_VERSION(0, 1, 0, 0),
  .create_device = gpu_create,
  .destroy_device = gpu_destroy,
  .enumerate_devices = gpu_enum,
  .check_compatibility = gpu_check,
  .query = NULL  // 旧的查询接口不再使用，由新的组件系统替代
};

// 使用新的组件系统注册
MODYN_REGISTER_COMPONENT_STRING(MODYN_COMPONENT_DEVICE, "dummy_gpu_device", &dummy_gpu_device_interface);

// 备用注册入口：若构造器在某些平台未触发，则由宿主在 dlopen 后显式调用
void modyn_plugin_register(void) {
  // 检查组件是否已经注册
  modyn_component_interface_t *existing = modyn_find_component(
      MODYN_COMPONENT_DEVICE, "dummy_gpu_device");
  
  if (existing) {
    printf("✓ Dummy GPU device already registered via constructor\n");
    // 如果已经注册，只需要设置私有数据
    existing->private_data = (void*)&g_dummy_gpu_driver;
    return;
  }
  
  // 设置私有数据指向现有驱动（需要去掉const限定符）
  dummy_gpu_device_interface.private_data = (void*)&g_dummy_gpu_driver;
  
  // 使用新的组件系统注册
  modyn_status_e status = modyn_register_component(
      MODYN_COMPONENT_DEVICE,
      "dummy_gpu_device",
      &dummy_gpu_device_interface,
      MODYN_COMPONENT_SOURCE_PLUGIN
  );
  
  if (status == MODYN_SUCCESS) {
    printf("✓ Dummy GPU device registered successfully\n");
  } else {
    printf("✗ Failed to register dummy GPU device: %d\n", status);
  }
}


