#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "modyn.h"
#include "../../component_manager.h"

// 一个最简单的 dummy 设备，实现最基本的同步推理：
// 输入 -> 直接复制到输出，类型与形状不变。

static modyn_status_e dummy_initialize(modyn_inference_device_handle_t device,
                                       const modyn_device_context_config_t *config) {
    (void)device; (void)config;
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_finalize(modyn_inference_device_handle_t device) {
    (void)device;
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_load_model(modyn_inference_device_handle_t device,
                                       modyn_model_weight_handle_t weights,
                                       modyn_model_instance_handle_t *instance) {
    (void)device; (void)weights;
    *instance = (modyn_model_instance_handle_t)0x1; // 虚拟句柄
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_unload_model(modyn_inference_device_handle_t device,
                                         modyn_model_instance_handle_t instance) {
    (void)device; (void)instance;
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_allocate_tensor(modyn_inference_device_handle_t device,
                                            const modyn_tensor_shape_t *shape,
                                            modyn_data_type_e dtype,
                                            modyn_tensor_data_t *tensor) {
    (void)device;
    if (!shape || !tensor) return MODYN_ERROR_INVALID_ARGUMENT;
    tensor->shape = *shape;
    tensor->dtype = dtype;
    tensor->size = MODYN_TENSOR_SIZE_BYTES(shape, dtype);
    tensor->mem_type = MODYN_MEMORY_INTERNAL;
    tensor->data = MODYN_MALLOC(tensor->size);
    if (!tensor->data) return MODYN_ERROR_MEMORY_ALLOCATION;
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_free_tensor(modyn_inference_device_handle_t device,
                                        modyn_tensor_data_t *tensor) {
    (void)device;
    if (!tensor) return MODYN_ERROR_INVALID_ARGUMENT;
    MODYN_FREE(tensor->data);
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_copy_tensor(modyn_inference_device_handle_t src_device,
                                        modyn_inference_device_handle_t dst_device,
                                        const modyn_tensor_data_t *src,
                                        modyn_tensor_data_t *dst) {
    (void)src_device; (void)dst_device;
    if (!src || !dst || !src->data || !dst->data) return MODYN_ERROR_INVALID_ARGUMENT;
    if (src->size != dst->size) return MODYN_ERROR_INVALID_ARGUMENT;
    memcpy(dst->data, src->data, src->size);
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_run_sync(modyn_inference_device_handle_t device,
                                     modyn_model_instance_handle_t instance,
                                     modyn_tensor_data_t *inputs,
                                     size_t num_inputs,
                                     modyn_tensor_data_t **outputs,
                                     size_t *num_outputs) {
    (void)device; (void)instance;
    if (!inputs || num_inputs == 0 || !outputs || !num_outputs) return MODYN_ERROR_INVALID_ARGUMENT;
    // 简单策略：将第一个输入复制到一个新输出
    *num_outputs = 1;
    *outputs = (modyn_tensor_data_t*)MODYN_MALLOC(sizeof(modyn_tensor_data_t));
    if (!*outputs) return MODYN_ERROR_MEMORY_ALLOCATION;
    modyn_tensor_data_t *out = &(*outputs)[0];
    out->shape = inputs[0].shape;
    out->dtype = inputs[0].dtype;
    out->size = inputs[0].size;
    out->mem_type = MODYN_MEMORY_INTERNAL;
    out->data = MODYN_MALLOC(out->size);
    if (!out->data) return MODYN_ERROR_MEMORY_ALLOCATION;
    memcpy(out->data, inputs[0].data, out->size);
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_get_performance(modyn_inference_device_handle_t device,
                                            modyn_device_performance_t *metrics) {
    (void)device;
    if (!metrics) return MODYN_ERROR_INVALID_ARGUMENT;
    metrics->peak_flops = 1.0f;
    metrics->memory_bandwidth = 1.0f;
    metrics->memory_size = MODYN_MB(64);
    metrics->available_memory = MODYN_MB(64);
    metrics->power_consumption = 1.0f;
    metrics->temperature = 40.0f;
    metrics->utilization = 0.01f;
    return MODYN_SUCCESS;
}

// 构造一个静态的 dummy 设备实例与操作表
static modyn_inference_device_ops_t g_dummy_ops = {
    .initialize = dummy_initialize,
    .finalize = dummy_finalize,
    .load_model = dummy_load_model,
    .unload_model = dummy_unload_model,
    .run_sync = dummy_run_sync,
    .run_async = NULL,
    .allocate_tensor = dummy_allocate_tensor,
    .free_tensor = dummy_free_tensor,
    .copy_tensor = dummy_copy_tensor,
    .get_performance = dummy_get_performance,
    .reset_stats = NULL,
    .clone_model_instance = NULL,
    .create_zero_copy_pool = NULL,
    .destroy_zero_copy_pool = NULL,
    .run_inference_zero_copy = NULL,
    .zero_copy_ops = NULL,
    .extensions = NULL
};

// 设备驱动：统一注册入口
static modyn_status_e dummy_create(int device_id, modyn_inference_device_handle_t *device) {
    (void)device_id;
    if (!device) return MODYN_ERROR_INVALID_ARGUMENT;
    static modyn_inference_device_t s_device;
    memset(&s_device, 0, sizeof(s_device));
    s_device.info.type = MODYN_DEVICE_CPU;
    strncpy(s_device.info.name, "Dummy Device", sizeof(s_device.info.name) - 1);
    strncpy(s_device.info.vendor, "Modyn", sizeof(s_device.info.vendor) - 1);
    strncpy(s_device.info.driver_version, "0.1.0", sizeof(s_device.info.driver_version) - 1);
    s_device.ops = &g_dummy_ops;
    *device = (modyn_inference_device_handle_t)&s_device;
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_destroy(modyn_inference_device_handle_t device) {
    (void)device; return MODYN_SUCCESS;
}

static modyn_status_e dummy_enumerate(modyn_device_info_t *devices, size_t max, size_t *count) {
    if (!devices || !count || max == 0) return MODYN_ERROR_INVALID_ARGUMENT;
    devices[0].type = MODYN_DEVICE_CPU;
    strncpy(devices[0].name, "Dummy Device", sizeof(devices[0].name)-1);
    strncpy(devices[0].vendor, "Modyn", sizeof(devices[0].vendor)-1);
    strncpy(devices[0].driver_version, "0.1.0", sizeof(devices[0].driver_version)-1);
    devices[0].device_id = 0;
    devices[0].numa_node = 0;
    *count = 1;
    return MODYN_SUCCESS;
}

static modyn_status_e dummy_check(const char *model_path, modyn_device_info_t *info, int *ok) {
    (void)model_path; (void)info; if (!ok) return MODYN_ERROR_INVALID_ARGUMENT; *ok = 1; return MODYN_SUCCESS;
}

static modyn_status_e dummy_query(const modyn_device_driver_t *driver, void *query_info) {
    (void)driver; (void)query_info;
    // 这里可以返回驱动的详细信息，如版本、能力等
    return MODYN_SUCCESS;
}

// 保留驱动结构用于向后兼容，但不再自动注册
static const modyn_device_driver_t g_dummy_driver = {
    .device_type = MODYN_DEVICE_CPU,
    .name = "dummy",
    .version = MODYN_COMPONENT_VERSION(0, 1, 0, 0),
    .create_device = dummy_create,
    .destroy_device = dummy_destroy,
    .enumerate_devices = dummy_enumerate,
    .check_compatibility = dummy_check,
    .query = dummy_query
};

// 为了保持向后兼容性，同时注册到旧的驱动系统
// 注意：不再使用 MODYN_REGISTER_DEVICE_DRIVER，改用新的组件注册系统

// 提供最小可运行的框架 API（示例）
MODYN_API modyn_status_e modyn_initialize(const modyn_framework_config_t *config) {
    (void)config;
    MODYN_LOG_INFO("Modyn framework initialized (dummy)");
    return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_shutdown(void) {
    MODYN_LOG_INFO("Modyn framework shutdown (dummy)");
    return MODYN_SUCCESS;
}

// 直接复用 dummy_run_sync 的逻辑以演示推理路径
MODYN_API modyn_status_e modyn_run_inference(modyn_model_handle_t model_handle,
                                            modyn_tensor_data_t *inputs,
                                            size_t num_inputs,
                                            modyn_tensor_data_t **outputs,
                                            size_t *num_outputs) {
    (void)model_handle;
    return dummy_run_sync(NULL, (modyn_model_instance_handle_t)0x1,
                          inputs, num_inputs, outputs, num_outputs);
}

// 提供最小的设备信息查询实现，便于测试
MODYN_API modyn_status_e modyn_get_device_info(modyn_inference_device_handle_t device,
                                              modyn_device_info_t *info) {
    if (!device || !info) return MODYN_ERROR_INVALID_ARGUMENT;
    const modyn_inference_device_t *dev = (const modyn_inference_device_t*)device;
    *info = dev->info;
    return MODYN_SUCCESS;
}

// ======================== 新的组件自动注册系统 ======================== //

// 直接让现有的驱动结构实现组件接口
// 通过适配器模式，将驱动接口转换为组件接口

// 注意：生命周期管理方法已移除，由组件自己负责

// 组件状态查询适配器
static modyn_component_status_e dummy_component_get_status(const void *component) {
    (void)component;
    // dummy设备总是可用的
    return MODYN_COMPONENT_ACTIVE;
}

// 组件能力检查适配器
static int dummy_component_supports_feature(const void *component, const char *feature) {
    (void)component;
    if (!feature) return 0;
    
    // 检查支持的功能
    if (strcmp(feature, "basic_inference") == 0) return 1;
    if (strcmp(feature, "tensor_ops") == 0) return 1;
    if (strcmp(feature, "memory_management") == 0) return 1;
    
    return 0;
}

// 组件能力描述适配器
static const char* dummy_component_get_capabilities(const void *component) {
    (void)component;
    return "Basic inference, Tensor operations, Memory management, CPU simulation";
}

// 组件查询适配器 - 修复类型不匹配问题
static modyn_status_e dummy_component_query_adapter(const void *component, void *query_info) {
    (void)component; (void)query_info;
    
    // 调用现有的查询逻辑，但需要适配新的接口
    printf("  [Dummy Component] Query executed\n");
    return MODYN_SUCCESS;
}

// 定义dummy设备组件接口 - 专注于管理功能，不重复生命周期管理
static modyn_component_interface_t dummy_device_interface = {
    .name = "dummy_device",
    .version = "0.1.0",
    .type = MODYN_COMPONENT_DEVICE,
    .source = MODYN_COMPONENT_SOURCE_BUILTIN,
    .query = dummy_component_query_adapter,  // 使用适配器函数
    .get_status = dummy_component_get_status,
    .supports_feature = dummy_component_supports_feature,
    .get_capabilities = dummy_component_get_capabilities,
    .private_data = NULL  // 稍后设置
};

// 使用字符串版本的自动注册宏注册dummy设备组件
MODYN_REGISTER_COMPONENT_STRING(MODYN_COMPONENT_DEVICE, "dummy_device", &dummy_device_interface);


