#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <float.h>
#include "core/model_manager.h"
#include "core/tensor.h"
#include "utils/logger.h"
#include "core/memory_pool.h"

/**
 * @brief Modyn 性能测试工具
 */

typedef struct {
    char* model_path;
    char* model_id;
    char* input_data_path;
    int iterations;
    int threads;
    int warmup_iterations;
    bool use_memory_pool;
    bool detailed_output;
    InferBackendType backend;
} BenchmarkConfig;

typedef struct {
    double min_latency;
    double max_latency;
    double avg_latency;
    double total_time;
    int total_iterations;
    int success_count;
    int error_count;
    double throughput;  // iterations per second
} BenchmarkStats;

typedef struct {
    ModelManager* manager;
    ModelHandle model;
    memory_pool_t memory_pool;     // 修改为直接使用memory_pool_t，而不是指针
    BenchmarkConfig* config;
    BenchmarkStats stats;
    int thread_id;
    pthread_barrier_t* start_barrier;
} ThreadData;

// 获取当前时间（毫秒）
static double get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// 创建测试输入张量
static Tensor create_test_input(memory_pool_t pool) {
    // 创建测试输入张量
    size_t size = 3 * 224 * 224 * 4; // float32
    memory_handle_t handle = memory_pool_alloc(pool, size, 0, "test_input");
    if (!handle) {
        return (Tensor){0};
    }
    
    // 填充随机数据
    float* data = (float*)memory_handle_get_ptr(handle);
    for (size_t i = 0; i < size / 4; i++) {
        data[i] = (float)rand() / RAND_MAX;
    }
    
    uint32_t dims[] = {1, 3, 224, 224};
    tensor_shape_t shape = tensor_shape_create(dims, 4);
    return tensor_from_data("test_input", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NCHW, data, size, false);
}

// 创建测试输出张量
static Tensor create_test_output(memory_pool_t pool) {
    // 创建测试输出张量
    size_t size = 1000 * 4; // float32, 1000 classes
    memory_handle_t handle = memory_pool_alloc(pool, size, 0, "test_output");
    if (!handle) {
        return (Tensor){0};
    }
    
    // 清零输出数据
    float* data = (float*)memory_handle_get_ptr(handle);
    memset(data, 0, size);
    
    uint32_t dims[] = {1, 1000};
    tensor_shape_t shape = tensor_shape_create(dims, 2);
    return tensor_from_data("test_output", TENSOR_TYPE_FLOAT32, &shape, TENSOR_FORMAT_NC, data, size, false);
}

// 释放测试张量
static void free_test_tensor(Tensor* tensor, memory_pool_t pool) {
    if (tensor && tensor->data && !tensor->owns_data) {
        // 如果张量不拥有数据，说明数据来自内存池
        // 这里简化处理，实际应该通过某种方式找到对应的handle
        // 暂时只释放张量结构，不释放数据
        tensor_free(tensor);
    }
}

// 单次推理性能测试
static double benchmark_single_inference(ModelHandle model, memory_pool_t pool) {
    // 创建测试数据
    Tensor input = create_test_input(pool);
    Tensor output = create_test_output(pool);
    
    if (!input.data || !output.data) {
        LOG_ERROR("Failed to create test tensors");
        free_test_tensor(&input, pool);
        free_test_tensor(&output, pool);
        return -1.0;
    }
    
    // 记录开始时间
    double start_time = get_current_time_ms();
    
    // 执行推理
    int result = model_infer(model, &input, 1, &output, 1);
    
    // 记录结束时间
    double end_time = get_current_time_ms();
    
    // 清理资源
    free_test_tensor(&input, pool);
    free_test_tensor(&output, pool);
    
    if (result != 0) {
        LOG_ERROR("Model inference failed");
        return -1.0;
    }
    
    return end_time - start_time;
}

// 线程测试函数
static void* thread_benchmark_func(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    BenchmarkConfig* config = data->config;
    
    // 等待所有线程准备好
    pthread_barrier_wait(data->start_barrier);
    
    double start_time = get_current_time_ms();
    double min_latency = 1e9;
    double max_latency = 0.0;
    double total_latency = 0.0;
    int success_count = 0;
    int error_count = 0;
    
    // 预热
    for (int i = 0; i < config->warmup_iterations; i++) {
        benchmark_single_inference(data->model, data->memory_pool);
    }
    
    // 正式测试
    for (int i = 0; i < config->iterations; i++) {
        double latency = benchmark_single_inference(data->model, data->memory_pool);
        
        if (latency > 0) {
            success_count++;
            total_latency += latency;
            if (latency < min_latency) min_latency = latency;
            if (latency > max_latency) max_latency = latency;
        } else {
            error_count++;
        }
        
        if (config->detailed_output && (i + 1) % 100 == 0) {
            printf("线程 %d: 完成 %d/%d 次推理\n", data->thread_id, i + 1, config->iterations);
        }
    }
    
    double end_time = get_current_time_ms();
    
    // 保存统计结果
    data->stats.min_latency = min_latency;
    data->stats.max_latency = max_latency;
    data->stats.avg_latency = success_count > 0 ? total_latency / success_count : 0.0;
    data->stats.total_time = end_time - start_time;
    data->stats.total_iterations = config->iterations;
    data->stats.success_count = success_count;
    data->stats.error_count = error_count;
    data->stats.throughput = success_count > 0 ? success_count * 1000.0 / data->stats.total_time : 0.0;
    
    return NULL;
}

// 运行性能测试
static int run_benchmark(BenchmarkConfig* config) {
    LOG_INFO("开始性能测试...");
    LOG_INFO("模型路径: %s", config->model_path);
    LOG_INFO("后端类型: %s", infer_engine_get_backend_name(config->backend));
    LOG_INFO("迭代次数: %d", config->iterations);
    LOG_INFO("线程数: %d", config->threads);
    LOG_INFO("预热次数: %d", config->warmup_iterations);
    LOG_INFO("使用内存池: %s", config->use_memory_pool ? "是" : "否");
    
    // 创建模型管理器
    ModelManager* manager = model_manager_create();
    if (!manager) {
        LOG_ERROR("Failed to create model manager");
        return -1;
    }
    
    // 创建内存池（如果需要）
    memory_pool_t memory_pool = NULL;
    if (config->use_memory_pool) {
        size_t tensor_size = 1 * 224 * 224 * 3 * sizeof(float);  // 输入张量大小
        memory_pool_config_t pool_config = {
            .type = MEMORY_POOL_CPU,
            .initial_size = tensor_size * 2,
            .max_size = tensor_size * config->threads * 4,
            .grow_size = tensor_size,
            .alignment = 32,
            .strategy = MEMORY_ALLOC_BEST_FIT,
            .enable_tracking = true,
            .enable_debug = false,
            .external_memory = NULL,
            .external_size = 0
        };
        
        memory_pool = memory_pool_create(&pool_config);
        if (!memory_pool) {
            printf("❌ 创建内存池失败\n");
            return -1;
        }
        printf("✅ 内存池创建成功\n");
    }
    
    // 加载模型
    ModelConfig model_config = {
        .model_path = config->model_path,
        .model_id = config->model_id,
        .backend = config->backend,
        .max_instances = config->threads,
        .enable_cache = false,
        .custom_config = NULL
    };
    ModelHandle model = model_manager_load(manager, config->model_path, &model_config);
    if (!model) {
        LOG_ERROR("Failed to load model");
        if (memory_pool) {
            memory_pool_destroy(memory_pool);
        }
        model_manager_destroy(manager);
        return -1;
    }
    
    // 预热
    if (config->warmup_iterations > 0) {
        LOG_INFO("预热阶段...");
        for (int i = 0; i < config->warmup_iterations; i++) {
            benchmark_single_inference(model, memory_pool);
        }
    }
    
    // 准备线程数据
    ThreadData* thread_data = calloc(config->threads, sizeof(ThreadData));
    pthread_t* threads = calloc(config->threads, sizeof(pthread_t));
    pthread_barrier_t start_barrier;
    
    if (!thread_data || !threads) {
                 LOG_ERROR("Failed to allocate thread data");
         model_manager_unload(manager, model);
         if (memory_pool) {
             memory_pool_destroy(memory_pool);
         }
         model_manager_destroy(manager);
         return -1;
    }
    
    // 初始化同步屏障
    pthread_barrier_init(&start_barrier, NULL, config->threads);
    
    // 创建线程
    for (int i = 0; i < config->threads; i++) {
        thread_data[i].manager = manager;
        thread_data[i].model = model;
        thread_data[i].memory_pool = memory_pool;
        thread_data[i].config = config;
        thread_data[i].thread_id = i;
        thread_data[i].start_barrier = &start_barrier;
        
        // 初始化统计信息
        thread_data[i].stats.min_latency = DBL_MAX;
        thread_data[i].stats.max_latency = 0.0;
        thread_data[i].stats.avg_latency = 0.0;
        thread_data[i].stats.total_time = 0.0;
        thread_data[i].stats.total_iterations = config->iterations / config->threads;
        thread_data[i].stats.success_count = 0;
        thread_data[i].stats.error_count = 0;
        
        if (pthread_create(&threads[i], NULL, thread_benchmark_func, &thread_data[i]) != 0) {
            LOG_ERROR("Failed to create thread %d", i);
            // 清理已创建的线程
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(thread_data);
            free(threads);
                         pthread_barrier_destroy(&start_barrier);
             model_manager_unload(manager, model);
             if (memory_pool) {
                 memory_pool_destroy(memory_pool);
             }
             model_manager_destroy(manager);
             return -1;
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < config->threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 汇总统计信息
    BenchmarkStats total_stats = {0};
    total_stats.min_latency = DBL_MAX;
    
    for (int i = 0; i < config->threads; i++) {
        total_stats.total_time += thread_data[i].stats.total_time;
        total_stats.total_iterations += thread_data[i].stats.total_iterations;
        total_stats.success_count += thread_data[i].stats.success_count;
        total_stats.error_count += thread_data[i].stats.error_count;
        
        if (thread_data[i].stats.min_latency < total_stats.min_latency) {
            total_stats.min_latency = thread_data[i].stats.min_latency;
        }
        if (thread_data[i].stats.max_latency > total_stats.max_latency) {
            total_stats.max_latency = thread_data[i].stats.max_latency;
        }
    }
    
    total_stats.avg_latency = total_stats.total_time / total_stats.success_count;
    total_stats.throughput = total_stats.success_count / (total_stats.total_time / 1000.0);
    
    // 打印结果
    printf("\n=== 性能测试结果 ===\n");
    printf("总迭代次数: %d\n", total_stats.total_iterations);
    printf("成功次数: %d\n", total_stats.success_count);
    printf("失败次数: %d\n", total_stats.error_count);
    printf("最小延迟: %.2f ms\n", total_stats.min_latency);
    printf("最大延迟: %.2f ms\n", total_stats.max_latency);
    printf("平均延迟: %.2f ms\n", total_stats.avg_latency);
    printf("总时间: %.2f ms\n", total_stats.total_time);
    printf("吞吐量: %.2f inferences/sec\n", total_stats.throughput);
    
    if (config->detailed_output) {
        printf("\n=== 详细信息 ===\n");
        for (int i = 0; i < config->threads; i++) {
            printf("线程 %d: 成功=%d, 失败=%d, 平均延迟=%.2f ms\n",
                   i, thread_data[i].stats.success_count, thread_data[i].stats.error_count,
                   thread_data[i].stats.avg_latency);
        }
    }
    
    // 打印内存池统计信息
    if (memory_pool) {
        memory_pool_stats_t pool_stats;
        if (memory_pool_get_stats(memory_pool, &pool_stats) == 0) {
            printf("\n=== 内存池统计 ===\n");
            printf("总大小: %zu bytes\n", pool_stats.total_size);
            printf("已使用: %zu bytes\n", pool_stats.used_size);
            printf("空闲: %zu bytes\n", pool_stats.free_size);
            printf("峰值使用: %zu bytes\n", pool_stats.peak_usage);
            printf("分配次数: %u\n", pool_stats.alloc_count);
            printf("释放次数: %u\n", pool_stats.free_count);
            printf("活跃块数: %u\n", pool_stats.active_blocks);
            printf("碎片率: %.2f%%\n", pool_stats.fragmentation * 100.0);
        }
    }
    
    // 清理资源
    pthread_barrier_destroy(&start_barrier);
         free(thread_data);
     free(threads);
     
     model_manager_unload(manager, model);
     if (memory_pool) {
         memory_pool_destroy(memory_pool);
     }
     model_manager_destroy(manager);
    
    LOG_INFO("性能测试完成");
    return 0;
}

// 打印使用说明
static void print_usage(const char* program_name) {
    printf("Modyn 性能测试工具\n");
    printf("\n");
    printf("用法: %s [选项]\n", program_name);
    printf("\n");
    printf("选项:\n");
    printf("  -m, --model <文件>      模型文件路径 (必需)\n");
    printf("  -i, --iterations <数量> 推理迭代次数 (默认: 100)\n");
    printf("  -t, --threads <数量>    并发线程数 (默认: 1)\n");
    printf("  -w, --warmup <数量>     预热迭代次数 (默认: 10)\n");
    printf("  -b, --backend <后端>    推理后端 (dummy/rknn/openvino, 默认: auto)\n");
    printf("  -p, --memory-pool       使用内存池\n");
    printf("  -v, --verbose           详细输出\n");
    printf("  -h, --help              显示帮助信息\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s -m model.rknn -i 1000 -t 4\n", program_name);
    printf("  %s -m model.onnx -i 500 -t 2 -p -v\n", program_name);
    printf("\n");
}

int main(int argc, char* argv[]) {
    // 默认配置
    BenchmarkConfig config = {0};
    config.iterations = 100;
    config.threads = 1;
    config.warmup_iterations = 10;
    config.backend = INFER_BACKEND_DUMMY;
    config.use_memory_pool = false;
    config.detailed_output = false;
    
    // 解析命令行参数
    static struct option long_options[] = {
        {"model", required_argument, 0, 'm'},
        {"iterations", required_argument, 0, 'i'},
        {"threads", required_argument, 0, 't'},
        {"warmup", required_argument, 0, 'w'},
        {"backend", required_argument, 0, 'b'},
        {"memory-pool", no_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "m:i:t:w:b:pvh", long_options, NULL)) != -1) {
        switch (c) {
            case 'm':
                config.model_path = optarg;
                break;
            case 'i':
                config.iterations = atoi(optarg);
                break;
            case 't':
                config.threads = atoi(optarg);
                break;
            case 'w':
                config.warmup_iterations = atoi(optarg);
                break;
            case 'b':
                if (strcmp(optarg, "dummy") == 0) {
                    config.backend = INFER_BACKEND_DUMMY;
                } else if (strcmp(optarg, "rknn") == 0) {
                    config.backend = INFER_BACKEND_RKNN;
                } else if (strcmp(optarg, "openvino") == 0) {
                    config.backend = INFER_BACKEND_OPENVINO;
                } else if (strcmp(optarg, "auto") == 0) {
                    config.backend = INFER_BACKEND_UNKNOWN;
                }
                break;
            case 'p':
                config.use_memory_pool = true;
                break;
            case 'v':
                config.detailed_output = true;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }
    
    // 验证参数
    if (!config.model_path) {
        printf("❌ 必须指定模型文件\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (config.iterations <= 0 || config.threads <= 0) {
        printf("❌ 迭代次数和线程数必须大于0\n");
        return 1;
    }
    
    // 自动检测后端
    if (config.backend == INFER_BACKEND_UNKNOWN) {
        config.backend = infer_engine_detect_backend(config.model_path);
    }
    
    // 设置模型ID
    const char* filename = strrchr(config.model_path, '/');
    config.model_id = strdup(filename ? filename + 1 : config.model_path);
    
    // 初始化日志系统
    logger_init(config.detailed_output ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO, NULL);
    logger_set_console_output(true);
    
    // 运行测试
    int result = run_benchmark(&config);
    
    // 清理
    if (config.model_id) free(config.model_id);
    logger_cleanup();
    
    return result;
} 