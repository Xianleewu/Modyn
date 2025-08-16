#include "core/inference_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/**
 * @brief 虚拟推理引擎结构
 */
typedef struct {
    InferBackendType backend;
    bool model_loaded;
    uint32_t input_count;
    uint32_t output_count;
    Tensor* input_info;
    Tensor* output_info;
} DummyEngine;

static InferEngine dummy_create(const InferEngineConfig* config) {
    DummyEngine* engine = malloc(sizeof(DummyEngine));
    if (!engine) {
        return NULL;
    }

    engine->backend = INFER_BACKEND_DUMMY;
    engine->model_loaded = false;
    engine->input_count = 0;
    engine->output_count = 0;
    engine->input_info = NULL;
    engine->output_info = NULL;

    printf("[Dummy] 创建推理引擎\n");

    return (InferEngine)engine;
}

static void dummy_destroy(InferEngine engine) {
    if (!engine) {
        return;
    }

    DummyEngine* dummy = (DummyEngine*)engine;

    if (dummy->input_info) {
        free(dummy->input_info);
    }
    if (dummy->output_info) {
        free(dummy->output_info);
    }

    free(dummy);
    printf("[Dummy] 销毁推理引擎\n");
}

static int dummy_load_model(InferEngine engine, const char* model_path,
                           const void* model_data, size_t model_size) {
    if (!engine || !model_path) {
        return -1;
    }

    DummyEngine* dummy = (DummyEngine*)engine;

    printf("[Dummy] 加载模型: %s\n", model_path);

    // 模拟加载时间
    usleep(100000); // 100ms

    // 设置虚拟的输入输出信息
    dummy->input_count = 1;
    dummy->output_count = 1;

    dummy->input_info = malloc(sizeof(Tensor));
    dummy->output_info = malloc(sizeof(Tensor));

    if (!dummy->input_info || !dummy->output_info) {
        return -1;
    }

    // 虚拟输入张量 (1, 3, 224, 224)
    dummy->input_info->name = strdup("input");
    dummy->input_info->dtype = TENSOR_TYPE_FLOAT32;
    dummy->input_info->shape.ndim = 4;
    dummy->input_info->shape.dims[0] = 1;
    dummy->input_info->shape.dims[1] = 3;
    dummy->input_info->shape.dims[2] = 224;
    dummy->input_info->shape.dims[3] = 224;
    dummy->input_info->format = TENSOR_FORMAT_NCHW;
    dummy->input_info->memory_type = TENSOR_MEMORY_CPU;
    dummy->input_info->data = NULL;
    dummy->input_info->size = 1 * 3 * 224 * 224 * sizeof(float);

    // 虚拟输出张量 (1, 1000)
    dummy->output_info->name = strdup("output");
    dummy->output_info->dtype = TENSOR_TYPE_FLOAT32;
    dummy->output_info->shape.ndim = 2;
    dummy->output_info->shape.dims[0] = 1;
    dummy->output_info->shape.dims[1] = 1000;
    dummy->output_info->format = TENSOR_FORMAT_NC;
    dummy->output_info->memory_type = TENSOR_MEMORY_CPU;
    dummy->output_info->data = NULL;
    dummy->output_info->size = 1 * 1000 * sizeof(float);

    dummy->model_loaded = true;

    printf("[Dummy] 模型加载完成\n");
    return 0;
}

static int dummy_unload_model(InferEngine engine) {
    if (!engine) {
        return -1;
    }

    DummyEngine* dummy = (DummyEngine*)engine;
    dummy->model_loaded = false;

    printf("[Dummy] 卸载模型\n");
    return 0;
}

static int dummy_get_input_info(InferEngine engine, uint32_t index, Tensor* tensor_info) {
    if (!engine || !tensor_info) {
        return -1;
    }

    DummyEngine* dummy = (DummyEngine*)engine;

    if (!dummy->model_loaded || index >= dummy->input_count) {
        return -1;
    }

    *tensor_info = dummy->input_info[index];
    return 0;
}

static int dummy_get_output_info(InferEngine engine, uint32_t index, Tensor* tensor_info) {
    if (!engine || !tensor_info) {
        return -1;
    }

    DummyEngine* dummy = (DummyEngine*)engine;

    if (!dummy->model_loaded || index >= dummy->output_count) {
        return -1;
    }

    *tensor_info = dummy->output_info[index];
    return 0;
}

static int dummy_infer(InferEngine engine, const Tensor* inputs, uint32_t input_count,
                      Tensor* outputs, uint32_t output_count) {
    if (!engine || !inputs || !outputs) {
        return -1;
    }

    DummyEngine* dummy = (DummyEngine*)engine;

    if (!dummy->model_loaded) {
        return -2;
    }

    if (input_count != dummy->input_count || output_count != dummy->output_count) {
        return -3;
    }

    printf("[Dummy] 执行推理...\n");

    // 模拟推理时间
    usleep(10000); // 10ms

    // 简单的虚拟输出生成
    for (uint32_t i = 0; i < output_count; i++) {
        if (outputs[i].data && outputs[i].size > 0) {
            float* output_data = (float*)outputs[i].data;
            uint32_t element_count = outputs[i].size / sizeof(float);

            // 生成虚拟数据
            for (uint32_t j = 0; j < element_count; j++) {
                output_data[j] = (float)(rand() % 1000) / 1000.0f;
            }
        }
    }

    printf("[Dummy] 推理完成\n");
    return 0;
}

static uint32_t dummy_get_input_count(InferEngine engine) {
    if (!engine) {
        return 0;
    }

    DummyEngine* dummy = (DummyEngine*)engine;
    return dummy->input_count;
}

static uint32_t dummy_get_output_count(InferEngine engine) {
    if (!engine) {
        return 0;
    }

    DummyEngine* dummy = (DummyEngine*)engine;
    return dummy->output_count;
}

static InferBackendType dummy_get_backend_type(InferEngine engine) {
    return INFER_BACKEND_DUMMY;
}

static const char* dummy_get_version(InferEngine engine) {
    return "DummyEngine v1.0.0";
}

// 操作接口
static const InferEngineOps dummy_ops = {
    .create = dummy_create,
    .destroy = dummy_destroy,
    .load_model = dummy_load_model,
    .unload_model = dummy_unload_model,
    .get_input_info = dummy_get_input_info,
    .get_output_info = dummy_get_output_info,
    .infer = dummy_infer,
    .get_input_count = dummy_get_input_count,
    .get_output_count = dummy_get_output_count,
    .get_backend_type = dummy_get_backend_type,
    .get_version = dummy_get_version,
};

// 工厂
static const InferEngineFactory dummy_factory = {
    .backend = INFER_BACKEND_DUMMY,
    .name = "Dummy",
    .ops = &dummy_ops,
};

// 自动注册函数
__attribute__((constructor))
void register_dummy_backend(void) {
    infer_engine_register_factory(&dummy_factory);
    printf("[Dummy] 注册虚拟推理后端\n");
}
