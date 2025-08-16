#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "modyn.h"
#include "component_manager.h"

int main(void) {
    // 初始化组件管理器（确保所有组件都被加载）
    if (modyn_component_manager_init(NULL) != MODYN_SUCCESS) {
        printf("failed to initialize component manager\n");
        return 1;
    }
    
    // 获取 dummy 设备
    modyn_inference_device_handle_t device = NULL;
    if (modyn_create_inference_device(MODYN_DEVICE_CPU, 0, NULL, &device) != MODYN_SUCCESS) {
        printf("failed to create device\n");
        return 1;
    }

    // 初始化框架（可选）
    modyn_framework_config_t cfg = { .max_parallel_models = 1, .enable_async_inference = 0, .memory_pool_size = 64, .log_level = 1 };
    modyn_initialize(&cfg);

    // 构造一个输入张量
    const size_t num_elems = 8;
    modyn_tensor_shape_t shape = { .num_dims = 1, .dims = { num_elems } };
    modyn_tensor_data_t in = {0};
    in.shape = shape;
    in.dtype = MODYN_DATA_TYPE_UINT8;
    in.size = MODYN_TENSOR_SIZE_BYTES(&shape, in.dtype);
    in.mem_type = MODYN_MEMORY_INTERNAL;
    in.data = malloc(in.size);
    for (size_t i = 0; i < num_elems; ++i) ((unsigned char*)in.data)[i] = (unsigned char)i;

    // 运行推理（dummy：out = copy(in)）
    modyn_tensor_data_t *outs = NULL; size_t nouts = 0;
    if (modyn_run_inference((modyn_model_handle_t)0x0, &in, 1, &outs, &nouts) != MODYN_SUCCESS) {
        printf("inference failed\n");
        return 2;
    }

    // 打印输出
    printf("outs=%zu, first tensor bytes=", nouts);
    for (size_t i = 0; i < num_elems; ++i) printf("%u ", (unsigned)((unsigned char*)outs[0].data)[i]);
    printf("\n");

    // 释放
    free(in.data);
    if (outs) {
        if (outs[0].data) free(outs[0].data);
        free(outs);
    }
    modyn_shutdown();
    
    // 关闭组件管理器
    modyn_component_manager_shutdown();
    
    return 0;
}


