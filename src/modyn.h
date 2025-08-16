#ifndef __MODYN_H__
#define __MODYN_H__

#include <stddef.h>
#include <stdint.h>

// ========================== API Export Macro ========================== //
/**
 * @brief API export macro for shared library
 * 
 * Use this macro to decorate functions that should be exported as public API.
 * Example: MODYN_API int my_function(void);
 */
#ifdef _WIN32
#  ifdef MODYN_BUILDING_LIBRARY
#    define MODYN_API __declspec(dllexport)
#  else
#    define MODYN_API __declspec(dllimport)
#  endif
#else
#  define MODYN_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============ Auto-Register Support (Linux/ELF only) ============ //
#ifndef MODYN_CONSTRUCTOR
#  define MODYN_CONSTRUCTOR __attribute__((constructor))
#endif

// ========================== Common Utility Macros ====================== //

/**
 * @brief Compiler attributes and optimization hints
 */
#ifndef MODYN_LIKELY
#  if defined(__GNUC__) || defined(__clang__)
#    define MODYN_LIKELY(x)    __builtin_expect(!!(x), 1)
#    define MODYN_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#  else
#    define MODYN_LIKELY(x)    (x)
#    define MODYN_UNLIKELY(x)  (x)
#  endif
#endif

#ifndef MODYN_INLINE
#  if defined(__GNUC__) || defined(__clang__)
#    define MODYN_INLINE       static inline __attribute__((always_inline))
#  else
#    define MODYN_INLINE       static inline
#  endif
#endif

#ifndef MODYN_DEPRECATED
#  if defined(__GNUC__) || defined(__clang__)
#    define MODYN_DEPRECATED   __attribute__((deprecated))
#  else
#    define MODYN_DEPRECATED
#  endif
#endif

#ifndef MODYN_UNUSED
#  if defined(__GNUC__) || defined(__clang__)
#    define MODYN_UNUSED       __attribute__((unused))
#  else
#    define MODYN_UNUSED
#  endif
#endif

#ifndef MODYN_PACKED
#  if defined(__GNUC__) || defined(__clang__)
#    define MODYN_PACKED       __attribute__((packed))
#  else
#    define MODYN_PACKED
#  endif
#endif

#ifndef MODYN_ALIGN
#  if defined(__GNUC__) || defined(__clang__)
#    define MODYN_ALIGN(n)     __attribute__((aligned(n)))
#  else
#    define MODYN_ALIGN(n)
#  endif
#endif

/**
 * @brief Memory and size calculations
 */
#define MODYN_ARRAY_SIZE(arr)              (sizeof(arr) / sizeof((arr)[0]))
#define MODYN_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define MODYN_ALIGN_UP(x, align)           (((x) + (align) - 1) & ~((align) - 1))
#define MODYN_ALIGN_DOWN(x, align)         ((x) & ~((align) - 1))
#define MODYN_IS_ALIGNED(x, align)         (((x) & ((align) - 1)) == 0)

#define MODYN_MIN(a, b)                    ((a) < (b) ? (a) : (b))
#define MODYN_MAX(a, b)                    ((a) > (b) ? (a) : (b))
#define MODYN_CLAMP(x, min, max)           (MODYN_MIN(MODYN_MAX(x, min), max))

#define MODYN_KB(x)                        ((size_t)(x) * 1024)
#define MODYN_MB(x)                        ((size_t)(x) * 1024 * 1024)
#define MODYN_GB(x)                        ((size_t)(x) * 1024 * 1024 * 1024)

/**
 * @brief Bit manipulation macros
 */
#define MODYN_BIT(n)                       (1U << (n))
#define MODYN_SET_BIT(var, n)              ((var) |= MODYN_BIT(n))
#define MODYN_CLEAR_BIT(var, n)            ((var) &= ~MODYN_BIT(n))
#define MODYN_TOGGLE_BIT(var, n)           ((var) ^= MODYN_BIT(n))
#define MODYN_TEST_BIT(var, n)             (((var) & MODYN_BIT(n)) != 0)

#define MODYN_SET_MASK(var, mask)          ((var) |= (mask))
#define MODYN_CLEAR_MASK(var, mask)        ((var) &= ~(mask))
#define MODYN_TEST_MASK(var, mask)         (((var) & (mask)) == (mask))

/**
 * @brief Error handling and validation macros
 */
#define MODYN_CHECK_NULL(ptr) \
    do { if (MODYN_UNLIKELY((ptr) == NULL)) return MODYN_ERROR_INVALID_ARGUMENT; } while(0)

#define MODYN_CHECK_STATUS(expr) \
    do { \
        modyn_status_e _status = (expr); \
        if (MODYN_UNLIKELY(_status != MODYN_SUCCESS)) return _status; \
    } while(0)

#define MODYN_RETURN_IF_ERROR(expr) \
    do { \
        modyn_status_e _status = (expr); \
        if (MODYN_UNLIKELY(_status != MODYN_SUCCESS)) return _status; \
    } while(0)

#define MODYN_GOTO_IF_ERROR(expr, label) \
    do { \
        status = (expr); \
        if (MODYN_UNLIKELY(status != MODYN_SUCCESS)) goto label; \
    } while(0)

#define MODYN_VALIDATE_RANGE(val, min, max) \
    do { \
        if (MODYN_UNLIKELY((val) < (min) || (val) > (max))) \
            return MODYN_ERROR_INVALID_ARGUMENT; \
    } while(0)

/**
 * @brief Tensor shape and data utilities
 */
#define MODYN_TENSOR_SHAPE_MAX_DIMS        8

#define MODYN_SHAPE_ELEMENT_COUNT(shape) \
    ({ \
        size_t _count = 1; \
        for (size_t _i = 0; _i < (shape)->num_dims; _i++) { \
            _count *= (shape)->dims[_i]; \
        } \
        _count; \
    })

#define MODYN_TENSOR_SIZE_BYTES(shape, dtype) \
    (MODYN_SHAPE_ELEMENT_COUNT(shape) * modyn_get_data_type_size(dtype))

#define MODYN_TENSOR_IS_VALID_SHAPE(shape) \
    ((shape) != NULL && (shape)->num_dims > 0 && (shape)->num_dims <= MODYN_TENSOR_SHAPE_MAX_DIMS)

/**
 * @brief Data type size helpers
 */

/**
 * @brief Memory allocation wrappers with error checking
 */
#define MODYN_MALLOC(size) \
    ({ \
        void *_ptr = malloc(size); \
        if (MODYN_UNLIKELY(_ptr == NULL && (size) > 0)) { \
            return MODYN_ERROR_MEMORY_ALLOCATION; \
        } \
        _ptr; \
    })

#define MODYN_CALLOC(count, size) \
    ({ \
        void *_ptr = calloc(count, size); \
        if (MODYN_UNLIKELY(_ptr == NULL && (count) > 0 && (size) > 0)) { \
            return MODYN_ERROR_MEMORY_ALLOCATION; \
        } \
        _ptr; \
    })

#define MODYN_FREE(ptr) \
    do { \
        if ((ptr) != NULL) { \
            free(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)

/**
 * @brief Logging and debugging macros
 */
#ifdef MODYN_DEBUG
#  define MODYN_DEBUG_PRINT(fmt, ...) \
     fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#  define MODYN_DEBUG_PRINT(fmt, ...) do { } while(0)
#endif

#ifdef MODYN_ENABLE_LOGGING
#  define MODYN_LOG_INFO(fmt, ...) \
     fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)
#  define MODYN_LOG_WARN(fmt, ...) \
     fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)
#  define MODYN_LOG_ERROR(fmt, ...) \
     fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
#  define MODYN_LOG_INFO(fmt, ...)  do { } while(0)
#  define MODYN_LOG_WARN(fmt, ...)  do { } while(0)
#  define MODYN_LOG_ERROR(fmt, ...) do { } while(0)
#endif

/**
 * @brief Performance measurement macros
 */
#ifdef MODYN_ENABLE_PROFILING
#  include <time.h>
#  define MODYN_TIMER_START(timer_name) \
     struct timespec timer_name##_start, timer_name##_end; \
     clock_gettime(CLOCK_MONOTONIC, &timer_name##_start)

#  define MODYN_TIMER_END(timer_name) \
     clock_gettime(CLOCK_MONOTONIC, &timer_name##_end)

#  define MODYN_TIMER_ELAPSED_MS(timer_name) \
     (((timer_name##_end.tv_sec - timer_name##_start.tv_sec) * 1000.0) + \
      ((timer_name##_end.tv_nsec - timer_name##_start.tv_nsec) / 1000000.0))

#  define MODYN_PROFILE_SCOPE(name, code) \
     do { \
         MODYN_TIMER_START(name); \
         code; \
         MODYN_TIMER_END(name); \
         MODYN_LOG_INFO("Profile [%s]: %.3f ms", #name, MODYN_TIMER_ELAPSED_MS(name)); \
     } while(0)
#else
#  define MODYN_TIMER_START(timer_name)     do { } while(0)
#  define MODYN_TIMER_END(timer_name)       do { } while(0)
#  define MODYN_TIMER_ELAPSED_MS(timer_name) (0.0)
#  define MODYN_PROFILE_SCOPE(name, code)   do { code; } while(0)
#endif

/**
 * @brief String and format utilities
 */
#define MODYN_STRINGIFY(x)                 #x
#define MODYN_STRINGIFY_VALUE(x)           MODYN_STRINGIFY(x)

#define MODYN_SAFE_STRING(str)             ((str) ? (str) : "")
#define MODYN_STRING_EQUALS(a, b)          (strcmp((a), (b)) == 0)
#define MODYN_STRING_STARTS_WITH(str, prefix) \
    (strncmp((str), (prefix), strlen(prefix)) == 0)

// 版本号结构体定义
typedef struct __modyn_version {
  uint32_t major;                             // 主版本号
  uint32_t minor;                             // 次版本号
  uint32_t patch;                             // 补丁版本号
  uint32_t build;                             // 构建号
} modyn_version_t;

// 版本号辅助宏 - 使用静态初始化
#define MODYN_COMPONENT_VERSION(major, minor, patch, build) \
    (modyn_version_t){(major), (minor), (patch), (build)}

#define MODYN_COMPONENT_VERSION_MAJOR(ver) ((ver).major)
#define MODYN_COMPONENT_VERSION_MINOR(ver) ((ver).minor)
#define MODYN_COMPONENT_VERSION_PATCH(ver) ((ver).patch)
#define MODYN_COMPONENT_VERSION_BUILD(ver) ((ver).build)

/**
 * @brief Handle validation macros
 */
#define MODYN_VALIDATE_HANDLE(handle, type) \
    do { \
        if (MODYN_UNLIKELY((handle) == NULL)) { \
            MODYN_LOG_ERROR("Invalid " #type " handle: NULL"); \
            return MODYN_ERROR_INVALID_ARGUMENT; \
        } \
    } while(0)

/**
 * @brief Device capability checking macros
 */
#define MODYN_DEVICE_SUPPORTS(device_caps, capability) \
    (((device_caps) & (capability)) != 0)

#define MODYN_DEVICE_REQUIRES_ALL(device_caps, required_caps) \
    (((device_caps) & (required_caps)) == (required_caps))

/**
 * @brief Tensor format conversion helpers
 */
#define MODYN_TENSOR_OFFSET_4D(n, c, h, w, N, C, H, W) \
    ((n) * (C) * (H) * (W) + (c) * (H) * (W) + (h) * (W) + (w))

#define MODYN_TENSOR_OFFSET_3D(c, h, w, C, H, W) \
    ((c) * (H) * (W) + (h) * (W) + (w))

#define MODYN_TENSOR_OFFSET_2D(h, w, W) \
    ((h) * (W) + (w))

/**
 * @brief Version and build information macros
 */
#define MODYN_VERSION_MAJOR                2
#define MODYN_VERSION_MINOR                0
#define MODYN_VERSION_PATCH                0

#define MODYN_VERSION_STRING               MODYN_STRINGIFY_VALUE(MODYN_VERSION_MAJOR) "." \
                                       MODYN_STRINGIFY_VALUE(MODYN_VERSION_MINOR) "." \
                                       MODYN_STRINGIFY_VALUE(MODYN_VERSION_PATCH)

#define MODYN_VERSION_NUMBER              ((MODYN_VERSION_MAJOR << 16) | \
                                       (MODYN_VERSION_MINOR << 8) | \
                                       MODYN_VERSION_PATCH)

/**
 * @brief Build configuration detection
 */
#ifdef NDEBUG
#  define MODYN_BUILD_TYPE                "Release"
#else
#  define MODYN_BUILD_TYPE                "Debug"
#endif

#ifdef __GNUC__
#  define MODYN_COMPILER                  "GCC " __VERSION__
#elif defined(__clang__)
#  define MODYN_COMPILER                  "Clang " __clang_version__
#elif defined(_MSC_VER)
#  define MODYN_COMPILER                  "MSVC " MODYN_STRINGIFY_VALUE(_MSC_VER)
#else
#  define MODYN_COMPILER                  "Unknown"
#endif

/**
 * @brief API decoration for shared libraries
 */
#ifndef MODYN_API
#  if defined(_WIN32) && defined(MODYN_SHARED_LIBRARY)
#    ifdef MODYN_BUILDING_LIBRARY
#      define MODYN_API                   __declspec(dllexport)
#    else
#      define MODYN_API                   __declspec(dllimport)
#    endif
#  else
#    define MODYN_API
#  endif
#endif

/**
 * @brief Thread safety annotations (for static analysis)
 */
#if defined(__clang__) && defined(MODYN_THREAD_ANNOTATIONS)
#  define MODYN_THREAD_ANNOTATION(x)     __attribute__((x))
#  define MODYN_GUARDED_BY(x)            MODYN_THREAD_ANNOTATION(guarded_by(x))
#  define MODYN_REQUIRES(x)              MODYN_THREAD_ANNOTATION(requires_capability(x))
#  define MODYN_EXCLUDES(x)              MODYN_THREAD_ANNOTATION(locks_excluded(x))
#  define MODYN_ACQUIRE(x)               MODYN_THREAD_ANNOTATION(acquire_capability(x))
#  define MODYN_RELEASE(x)               MODYN_THREAD_ANNOTATION(release_capability(x))
#else
#  define MODYN_THREAD_ANNOTATION(x)
#  define MODYN_GUARDED_BY(x)
#  define MODYN_REQUIRES(x)
#  define MODYN_EXCLUDES(x)
#  define MODYN_ACQUIRE(x)
#  define MODYN_RELEASE(x)
#endif

/**
 * @file modyn_inference_engine.h
 * @brief Unified Neural Network Inference Framework
 *
 * Core design patterns applied:
 *  - Facade: modyn_engine.h as unified interface
 *  - Flyweight: ModelManager for shared weights
 *  - Factory Method: InferencerFactory for device-specific implementations
 *  - Composite: Pipeline for model orchestration
 *  - Bridge: Decoupling model from inference devices
 *  - Strategy: DeviceScheduler for load balancing
 *  - Adapter: MultimodalAdapter for input conversion
 */

#ifndef MODYN_INFERENCE_ENGINE_H
#define MODYN_INFERENCE_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========================== Type Definitions ========================== //

/**
 * @brief Status codes for API operations
 */
typedef enum __modyn_status {
  MODYN_SUCCESS = 0,
  MODYN_ERROR_INVALID_ARGUMENT,
  MODYN_ERROR_MODYN_MODEL_LOAD_FAILED,
  MODYN_ERROR_DEVICE_NOT_SUPPORTED,
  MODYN_ERROR_MEMORY_ALLOCATION,
  MODYN_ERROR_PIPELINE_EXECUTION,
  MODYN_ERROR_INVALID_MODEL_HANDLE,
  // ... other error codes
} modyn_status_e;

/**
 * @brief Supported device types for inference
 */
typedef enum __modyn_device_type {
  MODYN_DEVICE_AUTO = 0, // Automatic selection
  MODYN_DEVICE_CPU,
  MODYN_DEVICE_GPU,
  MODYN_DEVICE_NPU,
  MODYN_DEVICE_DSP,
  MODYN_DEVICE_TPU,
  MODYN_DEVICE_COUNT
} modyn_device_type_e;

/**
 * @brief Memory types for tensor allocation
 */
typedef enum __modyn_memory_type {
  MODYN_MEMORY_INTERNAL = 0,    // Framework-managed memory
  MODYN_MEMORY_EXTERNAL,        // User-provided memory
  MODYN_MEMORY_DMABUF,          // DMA buffer for zero-copy
  MODYN_MEMORY_SHARED,          // Shared across devices
  MODYN_MEMORY_DEVICE_NATIVE,   // Device-native memory (GPU/NPU)
  MODYN_MEMORY_ZERO_COPY,       // Zero-copy pre-allocated buffers
  MODYN_MEMORY_MAPPED_FILE      // Memory-mapped file descriptor
} modyn_memory_type_e;

/**
 * @brief Data types for tensor elements
 */
typedef enum __modyn_data_type {
  MODYN_DATA_TYPE_FLOAT32 = 0,
  MODYN_DATA_TYPE_FLOAT16,
  MODYN_DATA_TYPE_INT32,
  MODYN_DATA_TYPE_INT16,
  MODYN_DATA_TYPE_INT8,
  MODYN_DATA_TYPE_UINT8,
  // ... other data types
} modyn_data_type_e;

/**
 * @brief Get size in bytes for data type
 */
MODYN_INLINE size_t modyn_get_data_type_size(modyn_data_type_e dtype) {
    switch (dtype) {
        case MODYN_DATA_TYPE_FLOAT32: return 4;
        case MODYN_DATA_TYPE_FLOAT16: return 2;
        case MODYN_DATA_TYPE_INT32:   return 4;
        case MODYN_DATA_TYPE_INT16:   return 2;
        case MODYN_DATA_TYPE_INT8:    return 1;
        case MODYN_DATA_TYPE_UINT8:   return 1;
        default:                return 0;
    }
}

/**
 * @brief Tensor dimension information
 */
typedef struct __modyn_tensor_shape {
  size_t num_dims; // Number of dimensions
  size_t dims[8];  // Dimension sizes (max 8 dims)
} modyn_tensor_shape_t;

/**
 * @brief Tensor data specification
 */
typedef struct __modyn_tensor_data {
  void *data;              // Pointer to tensor data
  modyn_tensor_shape_t shape;    // Tensor dimensions
  modyn_data_type_e dtype;       // Data type of elements
  modyn_memory_type_e mem_type;  // Memory allocation type
  size_t size;             // Total size in bytes
} modyn_tensor_data_t;

/**
 * @brief Model metadata information
 */
typedef struct __modyn_model_metadata {
  char name[128];                  // Model name
  char version[32];                // Model version
  modyn_tensor_shape_t input_shape;      // Expected input shape
  modyn_tensor_shape_t output_shape;     // Expected output shape
  modyn_device_type_e preferred_device;  // Preferred execution device
  uint32_t required_features;      // Required hardware features bitmask
} modyn_model_metadata_t;

// ====================== Core Framework Handles ======================= //

typedef void *modyn_model_handle_t;         // Opaque handle to a model instance
typedef void *modyn_model_weight_handle_t;   // Opaque handle to shared model weights
typedef void *modyn_model_instance_handle_t; // Opaque handle to model instance (shared weights)
typedef void *modyn_pipeline_handle_t;      // Opaque handle to a pipeline
typedef void *modyn_memory_pool_handle_t;    // Opaque handle to a memory pool
typedef void *modyn_device_scheduler_handle_t; // Opaque handle to device scheduler
typedef void *modyn_async_request_handle_t;  // Opaque handle to async inference request
typedef void *modyn_inference_device_handle_t; // Opaque handle to inference device
typedef void *modyn_device_context_handle_t; // Opaque handle to device execution context
typedef void *modyn_model_loader_handle_t; // Opaque handle to model loader

// ========================== Memory Management ========================= //

/**
 * @brief Memory allocation callback interface
 */
typedef struct __modyn_memory_manager {
  // Allocate memory with specific alignment
  void *(*allocate)(size_t size, size_t alignment, modyn_memory_type_e type);

  // Free allocated memory
  void (*free)(void *ptr);

  // Register externally allocated memory
  modyn_status_e (*register_external)(void *ptr, size_t size);

  // Convert memory between devices (zero-copy if possible)
  modyn_status_e (*convert_device)(void *ptr, modyn_device_type_e from, modyn_device_type_e to);
} modyn_memory_manager_t;

/**
 * @brief Create a unified memory pool
 *
 * @param manager Memory management callbacks
 * @return modyn_memory_pool_handle_t Handle to created memory pool
 */
MODYN_API modyn_memory_pool_handle_t modyn_create_memory_pool(modyn_memory_manager_t *manager);

// ============ Memory Pool Provider (插件式内存池) ============ //
typedef struct __modyn_mempool_ops modyn_mempool_ops_t;
typedef struct __modyn_mempool_ops {
  modyn_memory_pool_handle_t (*create)(const modyn_memory_manager_t *manager);
  modyn_status_e (*destroy)(modyn_memory_pool_handle_t pool);
  const char *name;
  modyn_version_t version;                    // 内存池版本
  // 查询接口
  modyn_status_e (*query)(const modyn_mempool_ops_t *ops, void *query_info);
} modyn_mempool_ops_t;

// 注册内存池提供者
modyn_status_e modyn_register_mempool_provider(const modyn_mempool_ops_t *ops);

// 模型加载器驱动结构
typedef struct __modyn_model_loader_driver modyn_model_loader_driver_t;
typedef struct __modyn_model_loader_driver {
  const char *name;                           // 加载器名称
  modyn_version_t version;                    // 加载器版本
  const char *supported_formats[8];           // 支持的模型格式
  size_t num_supported_formats;               // 支持的格式数量
  // 核心接口
  modyn_status_e (*load_model)(const char *model_path, modyn_model_handle_t *model_handle);
  modyn_status_e (*unload_model)(modyn_model_handle_t model_handle);
  modyn_status_e (*get_metadata)(modyn_model_handle_t model_handle, modyn_model_metadata_t *metadata);
  // 查询接口
  modyn_status_e (*query)(const modyn_model_loader_driver_t *driver, void *query_info);
} modyn_model_loader_driver_t;

// 注册模型加载器驱动
modyn_status_e modyn_register_model_loader_driver(const modyn_model_loader_driver_t *driver);

// 自动注册宏
#define MODYN_REGISTER_MODEL_LOADER(driver_sym) \
  static void MODYN_CONSTRUCTOR modyn_autoreg_loader_##driver_sym(void) { \
    (void)modyn_register_model_loader_driver(&(driver_sym)); \
  }

// 自动注册宏
#define MODYN_REGISTER_MEMPOOL_PROVIDER(ops_sym) \
  static void MODYN_CONSTRUCTOR modyn_autoreg_mempool_##ops_sym(void){ (void)modyn_register_mempool_provider(&(ops_sym)); }

// 查询接口
modyn_status_e modyn_get_registered_mempool_count(size_t *count);
modyn_status_e modyn_list_registered_mempools(const modyn_mempool_ops_t *ops[], size_t max, size_t *out);
modyn_status_e modyn_find_mempool_by_name(const char *name, const modyn_mempool_ops_t **ops);

// 动态加载内存池插件
modyn_status_e modyn_load_mempool_from_file(const char *so_path);
modyn_status_e modyn_load_mempools_from_directory(const char *dir_path);


// ========================== Model Management ========================== //

/**
 * @brief Load a neural network model
 *
 * @param model_path Path to model file
 * @param preferred_device Preferred execution device
 * @param mem_pool Memory pool to use
 * @param model_handle [out] Handle to loaded model
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_load_model(const char *model_path, modyn_device_type_e preferred_device,
                          modyn_memory_pool_handle_t mem_pool, modyn_model_handle_t *model_handle);

/**
 * @brief Unload a model and release resources
 *
 * @param model_handle Handle to loaded model
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_unload_model(modyn_model_handle_t model_handle);

/**
 * @brief Get model metadata
 *
 * @param model_handle Handle to loaded model
 * @param metadata [out] Model metadata
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_model_metadata(modyn_model_handle_t model_handle,
                                  modyn_model_metadata_t *metadata);

// ========================= Inference Interface ======================== //

/**
 * @brief Perform inference on a model
 *
 * @param model_handle Handle to loaded model
 * @param inputs Input tensors
 * @param num_inputs Number of input tensors
 * @param outputs [out] Output tensors
 * @param num_outputs Number of output tensors
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_run_inference(modyn_model_handle_t model_handle, modyn_tensor_data_t *inputs,
                             size_t num_inputs, modyn_tensor_data_t **outputs,
                             size_t *num_outputs);

/**
 * @brief Perform inference on model instance (optimized for shared weights)
 *
 * @param instance_handle Handle to model instance
 * @param inputs Input tensors
 * @param num_inputs Number of input tensors
 * @param outputs [out] Output tensors
 * @param num_outputs [out] Number of output tensors
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_run_inference_instance(modyn_model_instance_handle_t instance_handle,
                                     modyn_tensor_data_t *inputs,
                                     size_t num_inputs,
                                     modyn_tensor_data_t **outputs,
                                     size_t *num_outputs);

// Pipeline创建、管理和查询相关函数声明和结构体定义已移动到 src/pipeline/modyn_pipeline.h



// Pipeline拓扑查询函数声明已移动到 src/pipeline/modyn_pipeline.h



// ======================== Multimodal Support ======================== //

/**
 * @brief Supported data modalities
 */
typedef enum __modyn_data_modality {
  MODYN_MODALITY_IMAGE = 0,
  MODYN_MODALITY_AUDIO,
  MODYN_MODALITY_TEXT,
  MODYN_MODALITY_VIDEO,
  MODYN_MODALITY_SENSOR
} modyn_data_modality_e;

/**
 * @brief Image data structure
 */
typedef struct __modyn_image_data {
  void *pixels; // Pixel data
  int width;    // Image width
  int height;   // Image height
  int channels; // Color channels (3=RGB, 4=RGBA, etc)
  int stride;   // Row stride in bytes
} modyn_image_data_t;

/**
 * @brief Audio data structure
 */
typedef struct __modyn_audio_data {
  float *samples;     // Audio samples
  size_t num_samples; // Number of samples
  int sample_rate;    // Sampling rate (Hz)
  int num_channels;   // Audio channels
} modyn_audio_data_t;

/**
 * @brief Text data structure
 */
typedef struct __modyn_text_data {
  const char *text; // Null-terminated string
  size_t length;    // String length
  int encoding;     // Text encoding
} modyn_text_data_t;

/**
 * @brief Convert multimodal data to tensors
 *
 * @param modality Input data modality
 * @param data Input data pointer
 * @param tensors [out] Converted tensors
 * @param num_tensors [out] Number of tensors created
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_convert_to_tensor(modyn_data_modality_e modality, void *data,
                                 modyn_tensor_data_t **tensors, size_t *num_tensors);

// ======================= Model Loading Architecture ================== //

/**
 * @brief Model data source type
 */
typedef enum __modyn_model_source_type {
  MODYN_MODEL_SOURCE_FILE = 0,       // File path
  MODYN_MODEL_SOURCE_BUFFER,         // Memory buffer
  MODYN_MODEL_SOURCE_STREAM,         // Data stream
  MODYN_MODEL_SOURCE_URL,            // Network URL
  MODYN_MODEL_SOURCE_EMBEDDED        // Embedded resource
} modyn_model_source_type_e;

/**
 * @brief Model encryption/compression type
 */
typedef enum __modyn_model_format {
  MODYN_MODEL_FORMAT_PLAIN = 0,      // Plain model file
  MODYN_MODEL_FORMAT_ENCRYPTED,      // Encrypted model file
  MODYN_MODEL_FORMAT_COMPRESSED,     // Compressed model file
  MODYN_MODEL_FORMAT_ENCRYPTED_COMPRESSED, // Both encrypted and compressed
  MODYN_MODEL_FORMAT_CUSTOM         // Custom format
} modyn_model_format_e;

/**
 * @brief Model load flags (bitmask) for special handling hints
 *
 * These flags are optional hints from upper layers to influence loader/device
 * behavior during model loading. Multiple flags can be OR-ed together.
 */
typedef enum __modyn_model_load_flag {
  MODYN_MODEL_LOAD_FLAG_NONE            = 0,        // No special handling
  MODYN_MODEL_LOAD_FLAG_LLM             = 1u << 0,  // Large Language Model
  MODYN_MODEL_LOAD_FLAG_VISION          = 1u << 1,  // Vision model (CV)
  MODYN_MODEL_LOAD_FLAG_SPEECH          = 1u << 2,  // Speech/ASR/TTS
  MODYN_MODEL_LOAD_FLAG_TRANSFORMER     = 1u << 3,  // Transformer-heavy
  MODYN_MODEL_LOAD_FLAG_QUANTIZED       = 1u << 4,  // Quantized model
  MODYN_MODEL_LOAD_FLAG_LOW_MEMORY_MODE = 1u << 5,  // Prefer low-memory path
  MODYN_MODEL_LOAD_FLAG_STREAMING       = 1u << 6,  // Prefer streaming usage
  MODYN_MODEL_LOAD_FLAG_CUSTOM_RESERVED = 1u << 15  // Reserved for custom
} modyn_model_load_flag_e;

typedef unsigned int modyn_model_load_flags_t;

/**
 * @brief Common loader configuration used via loader_config(void*)
 *
 * Pass a pointer to this struct in loader_config to convey standard flags.
 * If NULL, flags are treated as MODYN_MODEL_LOAD_FLAG_NONE.
 */
typedef struct __modyn_model_loader_config {
  modyn_model_load_flags_t flags;   // Bitmask of modyn_model_load_flag_e
  const void *custom;               // Optional user-defined extension
} modyn_model_loader_config_t;

/**
 * @brief Model loading status
 */
typedef enum __modyn_model_load_status {
  MODYN_MODEL_LOAD_SUCCESS = 0,
  MODYN_MODEL_LOAD_FAILED,
  MODYN_MODEL_LOAD_DECRYPT_FAILED,
  MODYN_MODEL_LOAD_DECOMPRESS_FAILED,
  MODYN_MODEL_LOAD_INVALID_FORMAT,
  MODYN_MODEL_LOAD_ACCESS_DENIED
} modyn_model_load_status_e;

/**
 * @brief Model data source
 */
typedef struct __modyn_model_data_source {
  modyn_model_source_type_e type;    // Source type

  union {
    // File path source
    struct {
      const char *path;        // File path
    } file;

    // Buffer source
    struct {
      const void *data;        // Buffer data
      size_t size;             // Buffer size
      int owns_data;           // Whether source owns the data
    } buffer;

    // Stream source
    struct {
      void *stream_handle;     // Stream handle
      size_t (*read_func)(void *handle, void *buffer, size_t size);
      int (*seek_func)(void *handle, long offset, int whence);
      void (*close_func)(void *handle);
    } stream;

    // URL source
    struct {
      const char *url;         // Network URL
      const char *headers;     // HTTP headers (optional)
      int timeout_seconds;     // Download timeout
    } url;

    // Embedded resource source
    struct {
      const char *resource_id; // Resource identifier
      const void *data;        // Embedded data pointer
      size_t size;             // Embedded data size
    } embedded;
  } source;

  void *user_data;             // User-defined data
} modyn_model_data_source_t;

/**
 * @brief Model metadata from loader
 */
typedef struct __modyn_model_load_info {
  modyn_model_data_source_t source;  // Original data source
  modyn_model_format_e format;       // Model format type
  size_t original_size;        // Original data size
  size_t processed_size;       // Processed data size
  char checksum[64];           // Model checksum/hash
  void *custom_metadata;       // Custom metadata
  modyn_model_load_flags_t applied_flags; // Flags effectively applied during load
} modyn_model_load_info_t;

/**
 * @brief Model data buffer
 */
typedef struct __modyn_model_data_buffer {
  void *data;                  // Model data pointer
  size_t size;                 // Data size in bytes
  modyn_memory_type_e memory_type;   // Memory type
  int owns_memory;             // Whether buffer owns the memory
} modyn_model_data_buffer_t;

/**
 * @brief Model loader operations (function pointers)
 */
typedef struct __modyn_model_loader_ops {
  // Check if loader can handle the model data source
  modyn_status_e (*can_load)(const modyn_model_data_source_t *source,
                         modyn_model_format_e *detected_format);

  // Load and process model from data source
  modyn_status_e (*load_model)(const modyn_model_data_source_t *source,
                           const void *config, // recommended: modyn_model_loader_config_t*
                           modyn_model_data_buffer_t *output_buffer,
                           modyn_model_load_info_t *load_info);

  // Load model from file path (legacy support)
  modyn_status_e (*load_model_from_file)(const char *model_path,
                                     const void *config, // recommended: modyn_model_loader_config_t*
                                     modyn_model_data_buffer_t *output_buffer,
                                     modyn_model_load_info_t *load_info);

  // Load model from buffer
  modyn_status_e (*load_model_from_buffer)(const void *buffer_data,
                                       size_t buffer_size,
                                        const void *config, // recommended: modyn_model_loader_config_t*
                                       modyn_model_data_buffer_t *output_buffer,
                                       modyn_model_load_info_t *load_info);

  // Free processed model data
  modyn_status_e (*free_model_data)(modyn_model_data_buffer_t *buffer);

  // Get loader information
  modyn_status_e (*get_loader_info)(char *name, size_t name_size,
                                char *version, size_t version_size);

  // Validate model integrity
  modyn_status_e (*validate_model)(const modyn_model_data_buffer_t *buffer,
                               const char *expected_checksum);

  // Custom extensions
  void *extensions;
} modyn_model_loader_ops_t;

/**
 * @brief Model loader structure
 */
typedef struct __modyn_model_loader {
  char name[64];                    // Loader name
  modyn_version_t version;          // Loader version
  modyn_model_format_e supported_formats[8]; // Supported formats array
  int priority;                     // Loader priority (higher = preferred)
  modyn_model_loader_ops_t *ops;          // Loader operations
  void *private_data;               // Private loader data
} modyn_model_loader_t;

// ======================= Model Loader Management ==================== //

/**
 * @brief Register a model loader
 *
 * @param loader Model loader to register
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_register_model_loader(const modyn_model_loader_t *loader);

/**
 * @brief Unregister a model loader
 *
 * @param loader_name Name of loader to unregister
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_unregister_model_loader(const char *loader_name);

// 自动注册模型加载器（构造器）- 使用新的驱动系统
#define MODYN_REGISTER_MODEL_LOADER_DRIVER(loader_sym) \
  static void MODYN_CONSTRUCTOR modyn_autoreg_loader_##loader_sym(void) { \
    (void)modyn_register_model_loader_driver(&(loader_sym)); \
  }

// 模型加载器查询接口
modyn_status_e modyn_get_registered_loader_count(size_t *count);
modyn_status_e modyn_list_registered_loaders(modyn_model_loader_t const **loaders, size_t max, size_t *out);
modyn_status_e modyn_find_registered_loader_by_name(const char *name, const modyn_model_loader_t **loader);

// 动态加载模型加载器插件
modyn_status_e modyn_load_model_loader_from_file(const char *so_path);
modyn_status_e modyn_load_model_loaders_from_directory(const char *dir_path);

/**
 * @brief Load model using registered loaders from data source
 *
 * @param source Model data source
 * @param loader_config Loader-specific configuration (can be NULL)
 * @param buffer [out] Loaded model data buffer
 * @param load_info [out] Model loading information
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_load_model_from_source(const modyn_model_data_source_t *source,
                                      const void *loader_config, // modyn_model_loader_config_t*
                                      modyn_model_data_buffer_t *buffer,
                                      modyn_model_load_info_t *load_info);

/**
 * @brief Load model using registered loaders from file path
 *
 * @param model_path Path to model file
 * @param loader_config Loader-specific configuration (can be NULL)
 * @param buffer [out] Loaded model data buffer
 * @param load_info [out] Model loading information
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_load_model_with_loader(const char *model_path,
                                     const void *loader_config, // modyn_model_loader_config_t*
                                     modyn_model_data_buffer_t *buffer,
                                     modyn_model_load_info_t *load_info);

/**
 * @brief Load model using registered loaders from buffer
 *
 * @param buffer_data Buffer containing model data
 * @param buffer_size Size of the buffer
 * @param loader_config Loader-specific configuration (can be NULL)
 * @param output_buffer [out] Loaded model data buffer
 * @param load_info [out] Model loading information
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_load_model_from_buffer(const void *buffer_data,
                                     size_t buffer_size,
                                     const void *loader_config, // modyn_model_loader_config_t*
                                     modyn_model_data_buffer_t *output_buffer,
                                     modyn_model_load_info_t *load_info);

/**
 * @brief Load model using registered loaders from URL
 *
 * @param url Model URL
 * @param headers HTTP headers (can be NULL)
 * @param timeout_seconds Download timeout
 * @param loader_config Loader-specific configuration (can be NULL)
 * @param buffer [out] Loaded model data buffer
 * @param load_info [out] Model loading information
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_load_model_from_url(const char *url,
                                  const char *headers,
                                  int timeout_seconds,
                                  const void *loader_config, // modyn_model_loader_config_t*
                                  modyn_model_data_buffer_t *buffer,
                                  modyn_model_load_info_t *load_info);

/**
 * @brief Free model data loaded by loader
 *
 * @param buffer Model data buffer to free
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_free_model_buffer(modyn_model_data_buffer_t *buffer);

/**
 * @brief Find suitable loader for model
 *
 * @param model_path Path to model file
 * @param loader_name [out] Name of suitable loader
 * @param name_size Size of loader_name buffer
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_find_model_loader(const char *model_path,
                                char *loader_name,
                                size_t name_size);

/**
 * @brief List all registered model loaders
 *
 * @param loaders [out] Array of loader names
 * @param max_loaders Maximum number of loaders to return
 * @param count [out] Actual number of loaders returned
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_list_model_loaders(char loaders[][64],
                                 size_t max_loaders,
                                 size_t *count);

/**
 * @brief Validate model file integrity
 *
 * @param model_path Path to model file
 * @param expected_checksum Expected checksum (can be NULL)
 * @param is_valid [out] Whether model is valid
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_validate_model_file(const char *model_path,
                                  const char *expected_checksum,
                                  int *is_valid);

// ===================== Model Data Source Helpers ==================== //

/**
 * @brief Create file data source
 *
 * @param file_path Path to model file
 * @param source [out] Created data source
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_create_file_source(const char *file_path,
                                 modyn_model_data_source_t *source);

/**
 * @brief Create buffer data source
 *
 * @param buffer_data Buffer containing model data
 * @param buffer_size Size of the buffer
 * @param owns_data Whether source should take ownership of data
 * @param source [out] Created data source
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_create_buffer_source(const void *buffer_data,
                                   size_t buffer_size,
                                   int owns_data,
                                   modyn_model_data_source_t *source);

/**
 * @brief Create URL data source
 *
 * @param url Model URL
 * @param headers HTTP headers (can be NULL)
 * @param timeout_seconds Download timeout
 * @param source [out] Created data source
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_create_url_source(const char *url,
                                const char *headers,
                                int timeout_seconds,
                                modyn_model_data_source_t *source);

/**
 * @brief Create embedded resource data source
 *
 * @param resource_id Resource identifier
 * @param resource_data Embedded resource data
 * @param resource_size Size of embedded resource
 * @param source [out] Created data source
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_create_embedded_source(const char *resource_id,
                                     const void *resource_data,
                                     size_t resource_size,
                                     modyn_model_data_source_t *source);

/**
 * @brief Destroy data source and free resources
 *
 * @param source Data source to destroy
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_destroy_data_source(modyn_model_data_source_t *source);

// ======================= Zero-Copy Memory Management ================ //

/**
 * @brief Zero-copy memory region descriptor
 */
typedef struct __modyn_zero_copy_memory_region {
  void *virtual_addr;          // Virtual address accessible by CPU
  uint64_t physical_addr;      // Physical address (if available)
  int fd;                      // File descriptor (for DMA buffer)
  size_t size;                 // Memory region size
  size_t alignment;            // Memory alignment requirement
  modyn_memory_type_e mem_type;      // Memory type
  modyn_device_type_e device_type;   // Target device type
  int is_coherent;             // Whether memory is cache coherent
  void *platform_handle;       // Platform-specific handle
} modyn_zero_copy_memory_region_t;

/**
 * @brief Pre-allocated buffer pool for zero-copy inference
 */
typedef struct __modyn_zero_copy_buffer_pool {
  modyn_zero_copy_memory_region_t *input_regions;   // Input buffer regions
  modyn_zero_copy_memory_region_t *output_regions;  // Output buffer regions
  size_t num_inputs;                          // Number of input buffers
  size_t num_outputs;                         // Number of output buffers
  int is_allocated;                           // Whether buffers are allocated
  modyn_model_instance_handle_t owner_instance;         // Owner model instance
  void *sync_objects;                         // Synchronization objects
} modyn_zero_copy_buffer_pool_t;

/**
 * @brief Zero-copy tensor data (extends modyn_tensor_data_t)
 */
typedef struct __modyn_zero_copy_tensor_data {
  modyn_tensor_data_t base;                    // Base tensor data
  modyn_zero_copy_memory_region_t *region;     // Zero-copy memory region
  int needs_sync;                        // Whether needs CPU/device sync
  void (*sync_to_device)(struct __modyn_zero_copy_tensor_data *tensor);
  void (*sync_to_cpu)(struct __modyn_zero_copy_tensor_data *tensor);
} modyn_zero_copy_tensor_data_t;

/**
 * @brief Zero-copy memory operations
 */
typedef struct __modyn_zero_copy_memory_ops {
  // Allocate zero-copy memory region
  modyn_status_e (*allocate_region)(modyn_inference_device_handle_t device,
                                 size_t size,
                                 size_t alignment,
                                 modyn_memory_type_e mem_type,
                                 modyn_zero_copy_memory_region_t *region);

  // Free zero-copy memory region
  modyn_status_e (*free_region)(modyn_inference_device_handle_t device,
                            modyn_zero_copy_memory_region_t *region);

  // Map memory region for CPU access
  modyn_status_e (*map_region)(modyn_zero_copy_memory_region_t *region,
                           void **cpu_addr);

  // Unmap memory region
  modyn_status_e (*unmap_region)(modyn_zero_copy_memory_region_t *region);

  // Synchronize memory between CPU and device
  modyn_status_e (*sync_region)(modyn_zero_copy_memory_region_t *region,
                            int to_device);

  // Get file descriptor for sharing
  modyn_status_e (*get_fd)(modyn_zero_copy_memory_region_t *region,
                       int *fd);
} modyn_zero_copy_memory_ops_t;

// ====================== Inference Device Architecture =============== //

/**
 * @brief Async inference callback
 */
typedef void (*modyn_async_callback_t)(modyn_async_request_handle_t request,
                                modyn_status_e status,
                                modyn_tensor_data_t *outputs,
                                size_t num_outputs,
                                void *user_data);

/**
 * @brief Device capability flags
 */
typedef enum __modyn_device_capability {
  MODYN_DEVICE_CAP_FLOAT32     = 1 << 0,  // Support FP32 precision
  MODYN_DEVICE_CAP_FLOAT16     = 1 << 1,  // Support FP16 precision
  MODYN_DEVICE_CAP_INT8        = 1 << 2,  // Support INT8 quantization
  MODYN_DEVICE_CAP_INT4        = 1 << 3,  // Support INT4 quantization
  MODYN_DEVICE_CAP_DYNAMIC     = 1 << 4,  // Support dynamic shapes
  MODYN_DEVICE_CAP_BATCH       = 1 << 5,  // Support batching
  MODYN_DEVICE_CAP_STREAMING   = 1 << 6,  // Support streaming inference
  MODYN_DEVICE_CAP_MULTIMODAL  = 1 << 7,  // Support multimodal models
  MODYN_DEVICE_CAP_TRANSFORMER = 1 << 8,  // Optimized for transformer models
  MODYN_DEVICE_CAP_CNN         = 1 << 9,  // Optimized for CNN models
  MODYN_DEVICE_CAP_RNN         = 1 << 10, // Optimized for RNN models
} modyn_device_capability_e;

/**
 * @brief Device performance metrics
 */
typedef struct __modyn_device_performance {
  float peak_flops;          // Peak FLOPS (GFLOPS)
  float memory_bandwidth;    // Memory bandwidth (GB/s)
  size_t memory_size;        // Total memory size (bytes)
  size_t available_memory;   // Available memory size (bytes)
  float power_consumption;   // Power consumption (watts)
  float temperature;         // Current temperature (celsius)
  float utilization;         // Current utilization (0.0-1.0)
} modyn_device_performance_t;

/**
 * @brief Device information
 */
typedef struct __modyn_device_info {
  modyn_device_type_e type;              // Device type
  char name[128];                  // Device name
  char vendor[64];                 // Vendor name
  char driver_version[32];         // Driver version
  uint32_t capabilities;           // Capability flags bitmask
  modyn_device_performance_t performance; // Performance metrics
  int device_id;                   // Device ID in system
  int numa_node;                   // NUMA node (for CPU devices)
} modyn_device_info_t;

/**
 * @brief Device execution context configuration
 */
typedef struct __modyn_device_context_config {
  int max_batch_size;        // Maximum batch size
  int num_threads;           // Number of threads (CPU/OpenMP)
  float memory_fraction;     // Memory fraction to use (0.0-1.0)
  int enable_profiling;      // Enable performance profiling
  int priority;              // Execution priority (0-highest, 10-lowest)
  void *platform_specific;   // Platform-specific configuration
} modyn_device_context_config_t;

/**
 * @brief Inference device operations (function pointers)
 */
typedef struct __modyn_inference_device_ops {
  // Device lifecycle
  modyn_status_e (*initialize)(modyn_inference_device_handle_t device,
                            const modyn_device_context_config_t *config);
  modyn_status_e (*finalize)(modyn_inference_device_handle_t device);

  // Model operations
  modyn_status_e (*load_model)(modyn_inference_device_handle_t device,
                           modyn_model_weight_handle_t weights,
                           modyn_model_instance_handle_t *instance);
  modyn_status_e (*unload_model)(modyn_inference_device_handle_t device,
                             modyn_model_instance_handle_t instance);

  // Inference operations
  modyn_status_e (*run_sync)(modyn_inference_device_handle_t device,
                         modyn_model_instance_handle_t instance,
                         modyn_tensor_data_t *inputs,
                         size_t num_inputs,
                         modyn_tensor_data_t **outputs,
                         size_t *num_outputs);

  modyn_status_e (*run_async)(modyn_inference_device_handle_t device,
                          modyn_model_instance_handle_t instance,
                          modyn_tensor_data_t *inputs,
                          size_t num_inputs,
                          modyn_async_callback_t callback,
                          void *user_data,
                          modyn_async_request_handle_t *request);

  // Memory operations
  modyn_status_e (*allocate_tensor)(modyn_inference_device_handle_t device,
                                const modyn_tensor_shape_t *shape,
                                modyn_data_type_e dtype,
                                modyn_tensor_data_t *tensor);
  modyn_status_e (*free_tensor)(modyn_inference_device_handle_t device,
                            modyn_tensor_data_t *tensor);
  modyn_status_e (*copy_tensor)(modyn_inference_device_handle_t src_device,
                            modyn_inference_device_handle_t dst_device,
                            const modyn_tensor_data_t *src,
                            modyn_tensor_data_t *dst);

  // Device monitoring
  modyn_status_e (*get_performance)(modyn_inference_device_handle_t device,
                                modyn_device_performance_t *metrics);
  modyn_status_e (*reset_stats)(modyn_inference_device_handle_t device);

  // Model instance cloning (for weight sharing)
  modyn_status_e (*clone_model_instance)(modyn_inference_device_handle_t device,
                                      modyn_model_instance_handle_t source_instance,
                                      modyn_model_instance_handle_t *cloned_instance);

  // Zero-copy memory operations
  modyn_status_e (*create_zero_copy_pool)(modyn_inference_device_handle_t device,
                                      modyn_model_instance_handle_t instance,
                                      modyn_zero_copy_buffer_pool_t **pool);
  modyn_status_e (*destroy_zero_copy_pool)(modyn_inference_device_handle_t device,
                                       modyn_zero_copy_buffer_pool_t *pool);

  // Zero-copy inference operations
  modyn_status_e (*run_inference_zero_copy)(modyn_inference_device_handle_t device,
                                        modyn_model_instance_handle_t instance,
                                        modyn_zero_copy_buffer_pool_t *pool);

  // Zero-copy memory operations
  modyn_zero_copy_memory_ops_t *zero_copy_ops;

  // Platform-specific extensions
  void *extensions;
} modyn_inference_device_ops_t;

/**
 * @brief Abstract inference device structure
 */
typedef struct __modyn_inference_device {
  modyn_device_info_t info;                    // Device information
  modyn_inference_device_ops_t *ops;           // Device operations
  modyn_device_context_handle_t context;           // Execution context
  void *private_data;                    // Private device data
  int ref_count;                         // Reference counter
  int is_busy;                          // Device busy flag
} modyn_inference_device_t;

/**
 * @brief Model instance clone configuration
 */
typedef struct __modyn_clone_config {
  int enable_weight_sharing;     // Enable weight sharing in clone
  int max_concurrent_instances;  // Max concurrent cloned instances
  void *platform_params;         // Platform-specific parameters
} modyn_clone_config_t;

// ======================== Pipeline System ======================== //

// ======================== Pipeline Types ======================== //
// Pipeline相关类型定义已移动到 src/pipeline/modyn_pipeline.h
// 使用Pipeline功能时请包含： #include "pipeline/modyn_pipeline.h"

// Pipeline节点配置结构体定义已移动到 src/pipeline/modyn_pipeline.h

// ======================== Pipeline Advanced Functions ======================== //

// Pipeline相关函数声明和结构体定义已移动到 src/pipeline/modyn_pipeline.h



// Pipeline执行选项和统计函数声明已移动到 src/pipeline/modyn_pipeline.h

// ======================== Device Management ======================== //

/**
 * @brief Get available inference devices
 *
 * @param devices [out] Array of available device types
 * @param max_devices Maximum devices to return
 * @param count [out] Actual number of devices found
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_available_devices(modyn_device_type_e *devices, size_t max_devices,
                                     size_t *count);

/**
 * @brief Set device preference for a model
 *
 * @param model_handle Model handle
 * @param device Preferred device type
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_set_model_device(modyn_model_handle_t model_handle, modyn_device_type_e device);

// ==================== Device Factory & Management =================== //

/**
 * @brief Device factory for creating platform-specific devices
 */
typedef struct __modyn_device_factory {
  modyn_device_type_e device_type;
  const char *name;

  // Factory methods
  modyn_status_e (*create_device)(int device_id, modyn_inference_device_handle_t *device);
  modyn_status_e (*destroy_device)(modyn_inference_device_handle_t device);
  modyn_status_e (*enumerate_devices)(modyn_device_info_t *devices,
                                  size_t max_devices,
                                  size_t *count);
  modyn_status_e (*check_compatibility)(const char *model_path,
                                    modyn_device_info_t *device_info,
                                    int *is_compatible);
} modyn_device_factory_t;

/**
 * @brief Register a device factory
 *
 * @param factory Device factory to register
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_register_device_factory(const modyn_device_factory_t *factory);

/**
 * @brief Device driver abstraction（统一注册入口）
 */
typedef struct __modyn_device_driver modyn_device_driver_t;

typedef struct __modyn_device_driver {
  modyn_device_type_e device_type;            // 设备类型
  const char *name;                           // 驱动名
  modyn_version_t version;                    // 驱动版本
  // 创建/销毁设备
  modyn_status_e (*create_device)(int device_id, modyn_inference_device_handle_t *device);
  modyn_status_e (*destroy_device)(modyn_inference_device_handle_t device);
  // 枚举与兼容性
  modyn_status_e (*enumerate_devices)(modyn_device_info_t *devices, size_t max_devices, size_t *count);
  modyn_status_e (*check_compatibility)(const char *model_path, modyn_device_info_t *device_info, int *is_compatible);
  // 查询接口
  modyn_status_e (*query)(const modyn_device_driver_t *driver, void *query_info);
} modyn_device_driver_t;

/**
 * @brief 注册设备驱动（由 MODYN_REGISTER_DEVICE_DRIVER 自动调用）
 */
modyn_status_e modyn_register_device_driver(const modyn_device_driver_t *driver);

// 供各驱动在其实现文件中使用的自动注册宏
#define MODYN_REGISTER_DEVICE_DRIVER(driver_sym) \
  static void MODYN_CONSTRUCTOR modyn_autoreg_driver_##driver_sym(void) { \
    (void)modyn_register_device_driver(&(driver_sym)); \
  }

/**
 * @brief 驱动信息（对外查询用）
 */
typedef struct __modyn_device_driver_info {
  modyn_device_type_e device_type;
  char name[64];
} modyn_device_driver_info_t;

/**
 * @brief 获取已注册驱动数量
 */
modyn_status_e modyn_get_registered_driver_count(size_t *count);

/**
 * @brief 列出当前已注册驱动的基本信息
 */
modyn_status_e modyn_get_registered_drivers(modyn_device_driver_info_t *drivers,
                                            size_t max_count,
                                            size_t *out_count);

/**
 * @brief 通过名称查找驱动
 */
modyn_status_e modyn_find_device_driver_by_name(const char *name,
                                                const modyn_device_driver_t **out_driver);

/**
 * @brief 通过类型查找驱动（若有多个，返回第一个）
 */
modyn_status_e modyn_find_device_driver_by_type(modyn_device_type_e type,
                                                const modyn_device_driver_t **out_driver);

/**
 * @brief Create inference device by type and ID
 *
 * @param device_type Type of device to create
 * @param device_id Device ID (0 for first device of type)
 * @param config Device context configuration
 * @param device [out] Created device handle
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_create_inference_device(modyn_device_type_e device_type,
                                      int device_id,
                                      const modyn_device_context_config_t *config,
                                      modyn_inference_device_handle_t *device);

/**
 * @brief Destroy inference device
 *
 * @param device Device handle to destroy
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_destroy_inference_device(modyn_inference_device_handle_t device);

/**
 * @brief Get device information
 *
 * @param device Device handle
 * @param info [out] Device information
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_device_info(modyn_inference_device_handle_t device,
                              modyn_device_info_t *info);

/**
 * @brief Check if device supports model
 *
 * @param device Device handle
 * @param model_path Path to model file
 * @param is_supported [out] Whether model is supported
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_check_model_compatibility(modyn_inference_device_handle_t device,
                                        const char *model_path,
                                        int *is_supported);

/**
 * @brief Enumerate all available devices
 *
 * @param devices [out] Array of device information
 * @param max_devices Maximum number of devices to return
 * @param count [out] Actual number of devices found
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_enumerate_all_devices(modyn_device_info_t *devices,
                                    size_t max_devices,
                                    size_t *count);

/**
 * @brief Get device by optimal criteria
 *
 * @param model_path Model to optimize for
 * @param requirements Performance requirements
 * @param device [out] Optimal device handle
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_optimal_inference_device(const char *model_path,
                                           void *requirements,
                                           modyn_inference_device_handle_t *device);

/**
 * @brief Device resource management
 *
 * @param device Device handle
 * @param memory_limit Memory limit in bytes (0 = no limit)
 * @param thread_limit Thread limit (0 = no limit)
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_set_device_limits(modyn_inference_device_handle_t device,
                                size_t memory_limit,
                                int thread_limit);

/**
 * @brief Get current device resource usage
 *
 * @param device Device handle
 * @param performance [out] Current performance metrics
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_device_performance(modyn_inference_device_handle_t device,
                                     modyn_device_performance_t *performance);

// ======================= Dynamic Driver Loading ===================== //

/**
 * @brief 从共享库加载设备驱动（.so）。
 *        .so 内部应使用 MODYN_REGISTER_DEVICE_DRIVER 宏在构造器中自动注册。
 * @param so_path 共享库路径（绝对或相对）
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_load_device_driver_from_file(const char *so_path);

/**
 * @brief 从目录批量加载所有 .so 驱动
 * @param dir_path 目录路径
 * @return modyn_status_e 操作状态
 */
modyn_status_e modyn_load_device_drivers_from_directory(const char *dir_path);

// ======================= Model Instance Cloning ===================== //

/**
 * @brief Clone a model instance (with optional weight sharing)
 *
 * @param source_instance Source model instance to clone
 * @param config Clone configuration (can be NULL for default)
 * @param cloned_instance [out] Cloned model instance handle
 * @return modyn_status_e Operation status
 *   - MODYN_SUCCESS: Clone successful
 *   - MODYN_ERROR_DEVICE_NOT_SUPPORTED: Clone not supported by device
 *   - MODYN_ERROR_MEMORY_ALLOCATION: Insufficient memory for clone
 */
modyn_status_e modyn_clone_model_instance(modyn_model_instance_handle_t source_instance,
                                   const modyn_clone_config_t *config,
                                   modyn_model_instance_handle_t *cloned_instance);

/**
 * @brief Check if model instance supports cloning
 *
 * @param instance Model instance handle
 * @param supports_clone [out] Whether instance supports cloning
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_check_clone_support(modyn_model_instance_handle_t instance,
                                  int *supports_clone);

/**
 * @brief Get clone information for a model instance
 *
 * @param instance Model instance handle
 * @param is_cloned [out] Whether this instance is a clone
 * @param clone_count [out] Number of active clones (if original)
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_clone_info(modyn_model_instance_handle_t instance,
                             int *is_cloned,
                             int *clone_count);

// ======================= Zero-Copy Buffer Management ================ //

/**
 * @brief Create zero-copy buffer pool for model instance
 *
 * @param instance Model instance handle
 * @param pool [out] Created zero-copy buffer pool
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_create_zero_copy_buffer_pool(modyn_model_instance_handle_t instance,
                                           modyn_zero_copy_buffer_pool_t **pool);

/**
 * @brief Destroy zero-copy buffer pool
 *
 * @param pool Zero-copy buffer pool to destroy
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_destroy_zero_copy_buffer_pool(modyn_zero_copy_buffer_pool_t *pool);

/**
 * @brief Get input buffer region from pool
 *
 * @param pool Zero-copy buffer pool
 * @param input_index Input tensor index
 * @param region [out] Input buffer region
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_input_buffer_region(modyn_zero_copy_buffer_pool_t *pool,
                                      size_t input_index,
                                      modyn_zero_copy_memory_region_t **region);

/**
 * @brief Get output buffer region from pool
 *
 * @param pool Zero-copy buffer pool
 * @param output_index Output tensor index
 * @param region [out] Output buffer region
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_output_buffer_region(modyn_zero_copy_buffer_pool_t *pool,
                                       size_t output_index,
                                       modyn_zero_copy_memory_region_t **region);

/**
 * @brief Run zero-copy inference
 *
 * @param instance Model instance handle
 * @param pool Pre-allocated zero-copy buffer pool
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_run_inference_zero_copy(modyn_model_instance_handle_t instance,
                                      modyn_zero_copy_buffer_pool_t *pool);

/**
 * @brief Sync buffer region to device
 *
 * @param region Memory region to sync
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_sync_buffer_to_device(modyn_zero_copy_memory_region_t *region);

/**
 * @brief Sync buffer region to CPU
 *
 * @param region Memory region to sync
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_sync_buffer_to_cpu(modyn_zero_copy_memory_region_t *region);

/**
 * @brief Get buffer pool information
 *
 * @param pool Zero-copy buffer pool
 * @param num_inputs [out] Number of input buffers
 * @param num_outputs [out] Number of output buffers
 * @param total_size [out] Total allocated memory size
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_get_buffer_pool_info(modyn_zero_copy_buffer_pool_t *pool,
                                   size_t *num_inputs,
                                   size_t *num_outputs,
                                   size_t *total_size);

/**
 * @brief Check if instance supports zero-copy
 *
 * @param instance Model instance handle
 * @param supports_zero_copy [out] Whether zero-copy is supported
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_check_zero_copy_support(modyn_model_instance_handle_t instance,
                                      int *supports_zero_copy);

// ====================== Advanced Configuration ===================== //

/**
 * @brief Framework configuration options
 */
typedef struct __modyn_framework_config {
  int max_parallel_models;    // Max concurrent models
  int enable_async_inference; // Enable async execution
  int memory_pool_size;       // Default memory pool size (MB)
  int log_level;              // Verbosity level
} modyn_framework_config_t;

/**
 * @brief Initialize the inference framework
 *
 * @param config Framework configuration
 * @return modyn_status_e Initialization status
 */
modyn_status_e modyn_initialize(const modyn_framework_config_t *config);

/**
 * @brief Shutdown the framework and release all resources
 *
 * @return modyn_status_e Shutdown status
 */
modyn_status_e modyn_shutdown();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MODYN_INFERENCE_ENGINE_H

#ifdef __cplusplus
}
#endif

#endif /*__MODYN_H__*/
