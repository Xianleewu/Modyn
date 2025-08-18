#ifndef __MODYN_H__
#define __MODYN_H__

// ========================== 主框架头文件 ========================== //
// 包含所有必要的模块定义

#include "modyn_core.h"      // 核心工具和宏定义
#include "modyn_types.h"     // 基础数据类型
#include "modyn_component.h" // 组件管理系统
#include "modyn_device.h"    // 设备管理接口
#include "modyn_loader.h"    // 模型加载器接口

// ========================== 公共API函数 ========================== //
// 这些是框架的主要入口点

#ifdef __cplusplus
extern "C" {
#endif

// ========================== 框架生命周期 ========================== //
/**
 * @brief 初始化Modyn框架
 * @param config 框架配置
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_initialize(const void *config);

/**
 * @brief 关闭Modyn框架
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_shutdown(void);

// ========================== 模型管理 ========================== //
/**
 * @brief 加载模型
 * @param model_path 模型文件路径
 * @param preferred_device 首选设备类型
 * @param mem_pool 内存池句柄
 * @param loader 模型加载器句柄
 * @param loader_config 加载器配置
 * @param model_handle [out] 模型句柄
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_load_model(const char *model_path, 
                                          modyn_device_type_e preferred_device,
                                          modyn_memory_pool_handle_t mem_pool, 
                                          modyn_model_loader_handle_t loader,
                                          const void *loader_config, 
                                          modyn_model_handle_t *model_handle);

/**
 * @brief 卸载模型
 * @param model_handle 模型句柄
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_unload_model(modyn_model_handle_t model_handle);

/**
 * @brief 获取模型元数据
 * @param model_handle 模型句柄
 * @param metadata [out] 元数据
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_get_model_metadata(modyn_model_handle_t model_handle,
                                                  modyn_model_metadata_t *metadata);

// ========================== 推理接口 ========================== //
/**
 * @brief 执行推理
 * @param model_handle 模型句柄
 * @param inputs 输入张量
 * @param num_inputs 输入张量数量
 * @param outputs [out] 输出张量
 * @param num_outputs [out] 输出张量数量
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_run_inference(modyn_model_handle_t model_handle, 
                                             modyn_tensor_data_t *inputs,
                                             size_t num_inputs, 
                                             modyn_tensor_data_t **outputs,
                                             size_t *num_outputs);

// ========================== 内存池管理 ========================== //
/**
 * @brief 创建内存池
 * @param config 内存池配置
 * @param mem_pool [out] 内存池句柄
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_create_memory_pool(const void *config,
                                                   modyn_memory_pool_handle_t *mem_pool);

/**
 * @brief 销毁内存池
 * @param mem_pool 内存池句柄
 * @return 操作状态
 */
MODYN_API modyn_status_e modyn_destroy_memory_pool(modyn_memory_pool_handle_t mem_pool);

// ========================== 版本信息 ========================== //
/**
 * @brief 获取框架版本
 * @return 版本信息
 */
MODYN_API const modyn_version_t *modyn_get_version(void);

/**
 * @brief 获取框架构建信息
 * @return 构建信息字符串
 */
MODYN_API const char *modyn_get_build_info(void);

#ifdef __cplusplus
}
#endif

#endif // __MODYN_H__
