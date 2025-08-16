## Modyn

统一的跨设备神经网络推理组件，提供一致的 API、设备抽象、模型加载与管线编排能力。本工程已完成自原有组件到"Modyn"命名体系的全面迁移与统一。

### 系统架构概览

Modyn框架采用分层架构设计，从顶层API到底层硬件抽象，提供完整的推理流水线管理能力：

```
┌─────────────────────────────────────────────────────────────┐
│                    接入层 (Access Layer)                      │
│                C/C++/Python API                            │
└─────────────────────────────────────────────────────────────┘
                                │
┌─────────────────────────────────────────────────────────────┐
│                   管理层 (Management Layer)                   │
│                    推理流水线 (Inference Pipeline)            │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│  │ 过程1→模型1→过程2 │  │ 模型1→过程1→模型N │  │ 过程1→模型2→模型N→过程2 │    │
│  └─────────────┘    └─────────────┘    └─────────────┘    │
└─────────────────────────────────────────────────────────────┘
                                │
┌─────────────────────────────────────────────────────────────┐
│                    模型层 (Model Layer)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────┐│
│  │    LLM      │  │    OCR      │  │    CLIP     │  │传统模型││
│  │Text/Video/  │  │Image→Text   │  │Image/Text   │  │Image/ ││
│  │Audio/Vector │  │             │  │             │  │Text  ││
│  │Image/Doc    │  │             │  │             │  │      ││
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────┘│
└─────────────────────────────────────────────────────────────┘
                                │
┌─────────────────────────────────────────────────────────────┐
│                   抽象层 (Abstraction Layer)                  │
│                    硬件抽象 (Hardware Abstraction)            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────┐│
│  │模型加卸载    │  │模型I/O获取   │  │模型推理     │  │多实例 ││
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────┘│
└─────────────────────────────────────────────────────────────┘
                                │
┌─────────────────────────────────────────────────────────────┐
│                    推理层 (Inference Layer)                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │    RKNN     │  │   Openvino  │  │  OPENCL/TVM/MNN     │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                                │
┌─────────────────────────────────────────────────────────────┐
│                    硬件层 (Hardware Layer)                    │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │   RKNPU     │  │  INTEL NPU  │  │         GPU         │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 架构设计特点

- **分层解耦**: 每层职责明确，通过标准接口进行交互
- **多模态支持**: 支持文本、图像、视频、音频、向量、文档等多种数据类型
- **硬件抽象**: 统一的硬件抽象层，支持多种推理框架和硬件平台
- **流水线编排**: 灵活的推理流水线管理，支持复杂的模型组合和数据处理流程
- **跨平台兼容**: 支持RKNN、OpenVINO、OpenCL/TVM/MNN等多种推理框架

### 已实现的功能与特性

- 命名与导出规范
  - 统一前缀：宏使用 `MODYN_`，类型使用 `modyn_xxx_t`/`modyn_xxx_e`，函数使用 `modyn_xxx`
  - API 导出宏：`MODYN_API`（Windows 使用 `__declspec(dllexport/dllimport)`，其他平台使用默认可见性）

- 通用工具宏与基础能力（见 `src/modyn.h`）
  - 编译期提示与内联：`MODYN_LIKELY/UNLIKELY`、`MODYN_INLINE`、`MODYN_DEPRECATED`、`MODYN_ALIGN`、`MODYN_PACKED`
  - 内存/位运算/尺寸计算：`MODYN_ARRAY_SIZE`、`MODYN_ALIGN_UP`、`MODYN_BIT`、`MODYN_MIN/MAX/CLAMP`、`MODYN_KB/MB/GB`
  - 错误处理宏：`MODYN_CHECK_NULL`、`MODYN_CHECK_STATUS`、`MODYN_RETURN_IF_ERROR`、`MODYN_GOTO_IF_ERROR`
  - 日志与性能：`MODYN_LOG_INFO/WARN/ERROR`、`MODYN_TIMER_START/END/ELAPSED_MS`、`MODYN_PROFILE_SCOPE`
  - 字符串工具：`MODYN_SAFE_STRING`、`MODYN_STRING_EQUALS/STARTS_WITH`

- 张量与数据类型
  - 形状与张量：`modyn_tensor_shape_t`、`modyn_tensor_data_t`
  - 数据类型枚举：`modyn_data_type_e`（FP32/FP16/INT8/…），`modyn_get_data_type_size()`
  - 形状/大小辅助：`MODYN_TENSOR_SHAPE_MAX_DIMS`、`MODYN_SHAPE_ELEMENT_COUNT`、`MODYN_TENSOR_SIZE_BYTES`

- 设备抽象与能力
  - 设备类型：`modyn_device_type_e`（AUTO/CPU/GPU/NPU/DSP/TPU）
  - 设备能力：`modyn_device_capability_e`（FP16/INT8/DYNAMIC/BATCH/STREAMING/CNN/RNN…）
  - 设备信息与性能：`modyn_device_info_t`、`modyn_device_performance_t`
  - 设备操作表与设备对象：`modyn_inference_device_ops_t`、`modyn_inference_device_t`

- 设备管理与工厂
  - 工厂接口与注册：`modyn_device_factory_t`、`modyn_register_device_factory()`
  - 设备生命周期：`modyn_create_inference_device()`、`modyn_destroy_inference_device()`、`modyn_get_device_info()`
  - 能力/最优设备查询：`modyn_check_model_compatibility()`、`modyn_get_optimal_inference_device()`
  - 资源与性能：`modyn_set_device_limits()`、`modyn_get_device_performance()`

- 模型管理与管线
  - 加载/卸载：`modyn_load_model()`、`modyn_unload_model()`、`modyn_get_model_metadata()`
  - 推理接口：`modyn_run_inference()`、（共享权重实例）`modyn_run_inference_instance()`
  - 管线：`modyn_create_pipeline()`、`modyn_pipeline_add_node()`、`modyn_pipeline_connect_nodes()`、`modyn_run_pipeline()`

- 模型加载器体系
  - 数据源与格式：`modyn_model_data_source_t`、`modyn_model_format_e`、`modyn_model_load_status_e`
  - 加载器接口与注册：`modyn_model_loader_ops_t`、`modyn_model_loader_t`、`modyn_register_model_loader()` / `modyn_unregister_model_loader()`
  - 多来源加载：`modyn_load_model_from_source()`、`modyn_load_model_with_loader()`、`modyn_load_model_from_buffer()`、`modyn_load_model_from_url()`
  - 数据源工具：`modyn_create_file_source()`、`modyn_create_buffer_source()`、`modyn_create_url_source()`、`modyn_create_embedded_source()`、`modyn_destroy_data_source()`
  - 缓冲管理与校验：`modyn_free_model_buffer()`、`modyn_validate_model_file()`、`modyn_find_model_loader()`、`modyn_list_model_loaders()`

- 多模态数据支持
  - 模态与数据结构：`modyn_data_modality_e`、`modyn_image_data_t`、`modyn_audio_data_t`、`modyn_text_data_t`
  - 统一转换：`modyn_convert_to_tensor()`

- 零拷贝内存与高性能路径
  - 内存区域与缓冲池：`modyn_zero_copy_memory_region_t`、`modyn_zero_copy_buffer_pool_t`
  - 零拷贝接口：`modyn_create_zero_copy_buffer_pool()`、`modyn_destroy_zero_copy_buffer_pool()`、`modyn_get_input_buffer_region()`、`modyn_get_output_buffer_region()`、`modyn_run_inference_zero_copy()`、`modyn_sync_buffer_to_device()`、`modyn_sync_buffer_to_cpu()`、`modyn_check_zero_copy_support()`
  - 设备级零拷贝操作表：`modyn_zero_copy_memory_ops_t`

- 模型实例克隆与共享权重
  - 接口：`modyn_clone_model_instance()`、`modyn_check_clone_support()`、`modyn_get_clone_info()`
  - 配置：`modyn_clone_config_t`

- 框架生命周期
  - 初始化与关闭：`modyn_initialize()`、`modyn_shutdown()`

- 组件版本管理与查询
  - 版本结构：`modyn_version_t`（major.minor.patch.build）
  - 版本辅助宏：`MODYN_COMPONENT_VERSION()`、`MODYN_COMPONENT_VERSION_MAJOR()`等
  - 组件查询接口：所有组件都支持`query`方法，提供版本和能力信息
  - 自动注册：使用`__attribute__((constructor)`实现组件自动注册

- 推理流水线拓扑管理
  - 拓扑查询：`modyn_query_pipeline_topology()`、`modyn_free_pipeline_topology()`
  - 节点信息：`modyn_pipeline_node_info_t`（名称、模型句柄、输入输出数量、源/汇节点标识）
  - 边信息：`modyn_pipeline_edge_info_t`（源节点、目标节点、输入输出索引）
  - 完整拓扑：`modyn_pipeline_topology_t`（节点数组、边数组、数量统计）

- 高级Pipeline节点类型
  - **预处理节点**：`modyn_pipeline_add_preprocess_node()`，支持图像尺寸调整、归一化、颜色空间转换
  - **后处理节点**：`modyn_pipeline_add_postprocess_node()`，支持置信度过滤、NMS、结果格式化
  - **条件节点**：`modyn_pipeline_add_conditional_node()`，支持阈值判断、表达式计算、自定义条件函数
  - **循环节点**：`modyn_pipeline_add_loop_node()`，支持固定次数、条件循环、无限循环
  - **自定义节点**：`modyn_pipeline_add_custom_node()`，支持动态库加载、用户自定义处理逻辑
  - **节点注册**：`modyn_register_custom_node_type()`、`modyn_unregister_custom_node_type()`

- Pipeline执行控制与统计
  - 执行选项：`modyn_pipeline_set_execution_options()`，支持超时设置、重试次数、并行执行
  - 执行统计：`modyn_pipeline_get_execution_stats()`，提供节点执行次数、成功率、执行时间等统计信息
  - 节点统计：每个节点维护独立的执行统计（执行次数、成功/失败次数、执行时间）
  - 边控制：支持启用/禁用特定的数据流连接

### 设计模式

- Facade、Factory Method、Strategy、Bridge、Adapter、Composite、Flyweight 等模式贯穿框架设计。

### 示例程序与测试（已适配 Modyn 命名）

- **基础示例**：
  - `src/utility_macros_example.c`：工具宏与性能计时示例
  - `src/model_loader_example.c`：模型加载器注册/加载与多来源示例
  - `src/rknn_zero_copy_example.c`：RKNN 平台零拷贝路径示例
  - `src/rknn_device_example.c`：RKNN 设备与模型实例克隆示例

- **综合测试套件**：
  - `tests/test_all.c`：完整的框架功能测试，包括版本管理、组件查询、Pipeline拓扑等
  - 测试覆盖：工具宏、类型系统、内存管理、模型加载标志、基础功能、组件注册、Pipeline功能
  - 版本信息展示：显示所有注册组件的版本和能力信息
  - Pipeline拓扑验证：测试节点创建、连接、拓扑查询和执行

- **Pipeline高级功能示例**：
  - `src/pipeline_example.c`：演示复杂Pipeline构建，包括各种节点类型、条件分支、循环控制
  - 功能展示：预处理/后处理节点、条件判断、循环执行、自定义节点注册、拓扑查询、执行统计
  - 实际应用：图像分类Pipeline（预处理→模型推理→置信度检查→后处理→重试循环）

### 备注

- 头文件：`src/modyn.h`
- 代码风格：Linux C 风格，类型命名遵循 `typedef xxx xxx_t / xxx_e`
- 性能/内存：提供对齐、零拷贝、流式与批处理支持；日志与性能计时可按需启用

### 构建与测试

- **构建系统**：使用CMake构建，支持静态库和动态库
- **测试执行**：`make -C build && ./build/modyn_all_tests`
- **组件管理**：支持内置组件和动态加载的`.so`插件
- **插件目录**：可通过`MODYN_PLUGIN_DIR`环境变量配置插件搜索路径


