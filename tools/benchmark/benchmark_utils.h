#ifndef BENCHMARK_UTILS_H
#define BENCHMARK_UTILS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 性能测试结果结构
 */
typedef struct {
    char* model_path;
    char* backend_name;
    int thread_count;
    int iterations;
    int warmup_iterations;
    bool use_memory_pool;
    
    int total_success;
    int total_errors;
    double success_rate;
    double min_latency;
    double max_latency;
    double avg_latency;
    double median_latency;
    double p99_latency;
    double total_time;
    double total_throughput;
    double avg_throughput_per_thread;
    
    double max_memory_mb;
    double avg_cpu_usage;
} benchmark_result_t;

// 为了向后兼容，保留旧的类型别名
typedef benchmark_result_t BenchmarkResult;

/**
 * @brief 获取当前时间（毫秒）
 * 
 * @return double 时间戳（毫秒）
 */
double benchmark_get_time_ms(void);

/**
 * @brief 休眠指定毫秒数
 * 
 * @param milliseconds 毫秒数
 */
void benchmark_sleep_ms(int milliseconds);

/**
 * @brief 打印系统信息
 */
void benchmark_print_system_info(void);

/**
 * @brief 打印内存使用情况
 */
void benchmark_print_memory_usage(void);

/**
 * @brief 打印CPU使用率
 */
void benchmark_print_cpu_usage(void);

/**
 * @brief 生成性能测试报告
 * 
 * @param filename 报告文件名
 * @param result 测试结果
 */
void benchmark_generate_report(const char* filename, const BenchmarkResult* result);

/**
 * @brief 计算延迟百分位数
 * 
 * @param latencies 延迟数组
 * @param count 数组大小
 * @param median 输出中位数
 * @param p95 输出95%分位数
 * @param p99 输出99%分位数
 */
void benchmark_calculate_percentiles(double* latencies, int count, 
                                    double* median, double* p95, double* p99);

/**
 * @brief 打印进度条
 * 
 * @param current 当前进度
 * @param total 总数
 * @param elapsed_time 已用时间（毫秒）
 */
void benchmark_print_progress(int current, int total, double elapsed_time);

#ifdef __cplusplus
}
#endif

#endif // BENCHMARK_UTILS_H 