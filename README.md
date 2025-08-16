# 🧠 Modyn - 跨平台模型推理服务系统

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/your-org/modyn)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey)](https://github.com/your-org/modyn)

**Modyn** 是一个统一的模型推理服务框架，支持多种硬件后端（如 RKNN、OpenVINO、TensorRT 等），提供高效的模型加载、管理、推理与编排能力。通过抽象推理接口和组件化设计，实现模型在多平台上的透明部署与调用。

## 📌 核心特性

- **🔧 多平台支持**：统一接口支持 RKNN、OpenVINO、TensorRT 等多种推理后端
- **⚡ 高性能**：内存复用、模型共享实例、pipeline 组合优化
- **🎯 易用性**：统一推理接口、自动输入输出推理、丰富的脚手架工具
- **🧩 模块化**：清晰的抽象层，便于扩展和裁剪
- **🚀 并发支持**：多实例管理，支持并发推理
- **📊 监控管理**：提供 REST/gRPC 接口进行模型管理和监控
- **🔍 智能模型解析**：自动分析模型格式、输入输出要求，提供优化配置建议
- **💾 内存池管理**：支持外部内存、内部分配、zero-copy优化，提高内存利用率
- **🎭 多模态支持**：音频、视频、文本、图像等多种数据类型统一处理
- **🔄 预处理管道**：丰富的预处理操作库，支持自定义扩展和并行处理
- **🔌 插件系统**：动态加载、热更新、依赖管理，灵活扩展系统功能

## 🏗️ 架构概览

```
                +-------------------------+
                |     管理服务 API        |
                |   (REST/gRPC/CLI)       |
                +------------+------------+
                             |
                             v
                  +----------+-----------+
                  |     推理管理器       |
                  | ModelManager / Pipe  |
                  +----------+-----------+
                             |
            +-------------------------------+
            |                               |
     +------+--------+              +-------+------+
     |   推理引擎抽象   |              |  预处理/后处理工具  |
     | InferEngine API|              | Resize / NMS 等 |
     +------+--------+              +-------+------+
            |                               |
   +--------+-------+         +-------------+-------------+
   |   RKNN 推理器    |         |  OpenVINO 推理器           |
   | RknnInferEngine |         | OpenVINOInferEngine       |
   +-----------------+         +---------------------------+
```

## 🎯 新功能详解

### 🔍 智能模型解析

自动分析模型文件，提取元信息和最优配置：

```c
// 创建模型解析器
ModelParser parser = model_parser_create();

// 解析模型元信息
ModelMetadata metadata;
model_parser_parse_metadata(parser, "model.onnx", &metadata);

// 自动建议最优配置
InferEngineConfig config;
model_parser_suggest_config(parser, &metadata, INFER_BACKEND_ONNX, &config);
```

### 💾 内存池管理

高效的内存分配和管理：

```c
// 创建内存池
MemoryPoolConfig pool_config = {
    .type = MEMORY_POOL_CPU,
    .initial_size = 256 * 1024 * 1024,  // 256MB
    .strategy = MEMORY_ALLOC_BEST_FIT,
    .enable_tracking = true
};
MemoryPool pool = memory_pool_create(&pool_config);

// 分配内存
MemoryHandle handle = memory_pool_alloc(pool, 1024, 32, "tensor_data");
void* ptr = memory_handle_get_ptr(handle);
```

### 🎭 多模态支持

统一处理多种数据类型：

```c
// 创建多模态数据容器
MultiModalData* data = multimodal_data_create(3);

// 添加文本数据
ModalityData* text = modality_data_create(MODALITY_TEXT, DATA_FORMAT_UTF8, 
                                         "Hello World", 11);
multimodal_data_add(data, text);

// 添加图像数据
ModalityData* image = modality_data_create(MODALITY_IMAGE, DATA_FORMAT_RGB, 
                                          image_buffer, image_size);
multimodal_data_add(data, image);

// 添加音频数据
ModalityData* audio = modality_data_create(MODALITY_AUDIO, DATA_FORMAT_PCM, 
                                          audio_buffer, audio_size);
multimodal_data_add(data, audio);
```

### 🔄 预处理管道

灵活的预处理操作组合：

```c
// 创建预处理管道
PreprocessPipeline pipeline = preprocess_pipeline_create();

// 添加图像调整大小操作
PreprocessParams resize_params = {
    .type = PREPROCESS_RESIZE,
    .params.resize = {224, 224, INTERPOLATION_LINEAR}
};
PreprocessOp resize_op = preprocess_op_create(PREPROCESS_RESIZE, &resize_params);
preprocess_pipeline_add_op(pipeline, resize_op);

// 添加归一化操作
PreprocessParams norm_params = {
    .type = PREPROCESS_NORMALIZE,
    .params.normalize = {{0.485f, 0.456f, 0.406f}, {0.229f, 0.224f, 0.225f}, 3}
};
PreprocessOp norm_op = preprocess_op_create(PREPROCESS_NORMALIZE, &norm_params);
preprocess_pipeline_add_op(pipeline, norm_op);

// 执行预处理
preprocess_pipeline_execute(pipeline, &input_tensor, &output_tensor);
```

### 🔌 插件系统

动态加载和管理插件：

```c
// 创建插件工厂
PluginFactory factory = plugin_factory_create();

// 添加搜索路径
plugin_factory_add_search_path(factory, "./plugins");

// 发现并加载插件
plugin_factory_discover(factory, NULL, NULL);
Plugin plugin = plugin_factory_load(factory, "onnx_runtime");

// 创建推理引擎
InferEngine engine = plugin_factory_create_inference_engine(factory, 
                                                           INFER_BACKEND_ONNX, 
                                                           &config);
```

## 🚀 快速开始

### 系统要求

- Linux 系统（Ubuntu 18.04+）
- CMake 3.10+
- GCC 7.4+
- OpenCV 4.x （可选）

### 构建项目

```bash
# 克隆项目
git clone https://github.com/your-org/modyn.git
cd modyn

# 创建构建目录
mkdir build && cd build

# 配置构建选项
cmake .. -DENABLE_RKNN=ON -DENABLE_OPENVINO=ON -DENABLE_API=ON

# 编译
make -j$(nproc)

# 运行测试
ctest
```

### 基础使用示例

```c
#include "core/model_manager.h"
#include "core/inference_engine.h"

int main() {
    // 1. 初始化模型管理器
    ModelManager* manager = model_manager_create();
    
    // 2. 加载模型
    ModelHandle model = model_manager_load(manager, "yolo.rknn", NULL);
    if (!model) {
        printf("模型加载失败\n");
        return -1;
    }
    
    // 3. 准备输入张量
    Tensor input_tensor = prepare_tensor_from_image("input.jpg");
    Tensor output_tensor = {0};
    
    // 4. 执行推理
    int ret = model_infer(model, &input_tensor, &output_tensor);
    if (ret != 0) {
        printf("推理失败\n");
        return -1;
    }
    
    // 5. 解析输出
    DetectionResult* results = decode_detection_output(&output_tensor);
    printf("检测到 %d 个目标\n", results->count);
    
    // 6. 清理资源
    tensor_free(&input_tensor);
    tensor_free(&output_tensor);
    model_manager_unload(manager, model);
    model_manager_destroy(manager);
    
    return 0;
}
```

## 📦 平台支持

| 平台          | 支持状态 | 说明                 |
| ----------- | ---- | ------------------ |
| RKNN        | ✅    | 支持 RK3588/RK3568 等 |
| OpenVINO    | ✅    | 支持 CPU/GPU/NPU 等   |
| TensorRT    | 🔜   | 计划支持               |
| ONNXRuntime | 🔜   | 计划支持               |
| Dummy CPU   | ✅    | 默认调试后端             |

## 📁 项目结构

```
modyn/
├── core/                   # 推理接口抽象 & 管理器
│   ├── model_manager.c     # 模型管理器实现
│   ├── inference_engine.c  # 推理引擎抽象
│   └── tensor.c           # 张量管理
├── backend/               # 各平台推理器实现
│   ├── rknn/             # RKNN 推理器
│   ├── openvino/         # OpenVINO 推理器
│   └── dummy/            # 调试用虚拟推理器
├── pipeline/              # 编排逻辑实现
│   ├── pipeline_manager.c # 管道管理器
│   └── scheduler.c        # 调度器
├── utils/                 # 图像预处理、后处理、工具
│   ├── image_utils.c      # 图像处理工具
│   ├── nms.c             # NMS 后处理
│   └── memory_pool.c      # 内存池管理
├── api/                   # REST/gRPC 接口（可选）
│   ├── rest_server.c      # REST API 服务器
│   └── grpc_server.c      # gRPC 服务器
├── include/               # 公共头文件
│   └── modyn/            # 主要头文件
├── tools/                 # 示例脚手架、模型转换等
│   ├── model_converter/   # 模型转换工具
│   └── benchmark/         # 性能测试工具
├── examples/              # 示例代码
├── tests/                 # 单元测试
├── docs/                  # 文档
├── CMakeLists.txt         # CMake 构建文件
└── README.md             # 项目说明
```

## 🔧 编译选项

| 选项             | 默认值 | 说明              |
| -------------- | --- | --------------- |
| ENABLE_RKNN    | OFF | 启用 RKNN 后端     |
| ENABLE_OPENVINO| OFF | 启用 OpenVINO 后端  |
| ENABLE_TENSORRT| OFF | 启用 TensorRT 后端  |
| ENABLE_API     | OFF | 启用 REST/gRPC API |
| ENABLE_OPENCV  | OFF | 启用 OpenCV 支持   |
| ENABLE_TESTS   | ON  | 启用单元测试        |

## 📊 管理接口

### REST API 示例

```bash
# 加载模型
curl -X POST "http://localhost:8080/models" \
  -H "Content-Type: application/json" \
  -d '{"model_path": "yolo.rknn", "model_id": "yolo_v5"}'

# 查询模型状态
curl -X GET "http://localhost:8080/models/yolo_v5/status"

# 执行推理
curl -X POST "http://localhost:8080/models/yolo_v5/infer" \
  -H "Content-Type: application/json" \
  -d '{"input_data": "base64_encoded_image"}'

# 卸载模型
curl -X DELETE "http://localhost:8080/models/yolo_v5"
```

## 🧪 性能测试

```bash
# 运行性能测试
./build/tools/benchmark/benchmark_tool \
  --model yolo.rknn \
  --input test_image.jpg \
  --iterations 1000 \
  --threads 4
```

## 🤝 贡献指南

1. Fork 本项目
2. 创建您的特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交您的改动 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开一个 Pull Request

## 📄 许可证

本项目使用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 🙏 致谢

- [RKNN](https://github.com/rockchip-linux/rknn-toolkit2) - Rockchip 神经网络推理框架
- [OpenVINO](https://github.com/openvinotoolkit/openvino) - Intel 开放视觉推理和神经网络优化工具包
- [OpenCV](https://opencv.org/) - 开源计算机视觉库

## 📞 联系我们

- 项目主页：[https://github.com/your-org/modyn](https://github.com/your-org/modyn)
- 问题反馈：[https://github.com/your-org/modyn/issues](https://github.com/your-org/modyn/issues)
- 邮件：modyn@example.com

---

**Modyn** - 让模型推理变得简单而高效！ 🚀 