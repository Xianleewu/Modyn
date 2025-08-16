#include "core/inference_engine.h"
#include <stdio.h>

/**
 * @brief 注册表实现
 * 
 * 这个文件主要用于管理推理引擎的注册和初始化
 * 在程序启动时自动注册所有可用的后端
 */

// 前向声明注册函数
extern void register_dummy_backend(void);

#ifdef MODYN_ENABLE_RKNN
extern void register_rknn_backend(void);
#endif

#ifdef MODYN_ENABLE_OPENVINO
extern void register_openvino_backend(void);
#endif

#ifdef MODYN_ENABLE_TENSORRT
extern void register_tensorrt_backend(void);
#endif

/**
 * @brief 初始化所有后端
 * 
 * 这个函数在程序启动时被调用，用于注册所有编译时启用的后端
 */
__attribute__((constructor))
void initialize_all_backends(void) {
    printf("初始化 Modyn 推理引擎...\n");
    
    // 注册虚拟后端（调试用）
    register_dummy_backend();
    
#ifdef MODYN_ENABLE_RKNN
    register_rknn_backend();
#endif

#ifdef MODYN_ENABLE_OPENVINO
    register_openvino_backend();
#endif

#ifdef MODYN_ENABLE_TENSORRT
    register_tensorrt_backend();
#endif
    
    printf("所有后端初始化完成\n");
} 