#include "benchmark_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

/**
 * @brief 性能测试辅助函数实现
 */

double benchmark_get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void benchmark_sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}

void benchmark_print_system_info(void) {
    printf("=== 系统信息 ===\n");
    
    // CPU 信息
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        int cpu_count = 0;
        char* cpu_model = NULL;
        
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "processor", 9) == 0) {
                cpu_count++;
            } else if (strncmp(line, "model name", 10) == 0 && !cpu_model) {
                char* colon = strchr(line, ':');
                if (colon) {
                    cpu_model = strdup(colon + 2);
                    // 移除换行符
                    char* newline = strchr(cpu_model, '\n');
                    if (newline) *newline = '\0';
                }
            }
        }
        fclose(cpuinfo);
        
        printf("CPU: %s\n", cpu_model ? cpu_model : "Unknown");
        printf("CPU 核心数: %d\n", cpu_count);
        
        if (cpu_model) free(cpu_model);
    }
    
    // 内存信息
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        long total_mem = 0, free_mem = 0;
        
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line, "MemTotal: %ld kB", &total_mem);
            } else if (strncmp(line, "MemFree:", 8) == 0) {
                sscanf(line, "MemFree: %ld kB", &free_mem);
            }
        }
        fclose(meminfo);
        
        printf("总内存: %.2f GB\n", total_mem / 1024.0 / 1024.0);
        printf("可用内存: %.2f GB\n", free_mem / 1024.0 / 1024.0);
    }
    
    // 操作系统信息
    FILE* version = fopen("/proc/version", "r");
    if (version) {
        char line[512];
        if (fgets(line, sizeof(line), version)) {
            printf("内核版本: ");
            
            // 提取内核版本号
            char* start = strstr(line, "Linux version ");
            if (start) {
                start += 14;  // 跳过 "Linux version "
                char* end = strchr(start, ' ');
                if (end) {
                    *end = '\0';
                    printf("%s", start);
                }
            }
            printf("\n");
        }
        fclose(version);
    }
    
    printf("\n");
}

void benchmark_print_memory_usage(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        printf("=== 内存使用情况 ===\n");
        printf("最大常驻集大小: %.2f MB\n", usage.ru_maxrss / 1024.0);
        printf("页面错误次数: %ld (主要), %ld (次要)\n", 
               usage.ru_majflt, usage.ru_minflt);
        printf("自愿上下文切换: %ld\n", usage.ru_nvcsw);
        printf("非自愿上下文切换: %ld\n", usage.ru_nivcsw);
        printf("\n");
    }
}

void benchmark_print_cpu_usage(void) {
    static clock_t last_cpu_time = 0;
    static double last_wall_time = 0;
    
    clock_t current_cpu_time = clock();
    double current_wall_time = benchmark_get_time_ms();
    
    if (last_cpu_time != 0 && last_wall_time != 0) {
        double cpu_time_diff = (double)(current_cpu_time - last_cpu_time) / CLOCKS_PER_SEC;
        double wall_time_diff = (current_wall_time - last_wall_time) / 1000.0;
        
        if (wall_time_diff > 0) {
            double cpu_usage = (cpu_time_diff / wall_time_diff) * 100.0;
            printf("CPU 使用率: %.1f%%\n", cpu_usage);
        }
    }
    
    last_cpu_time = current_cpu_time;
    last_wall_time = current_wall_time;
}

void benchmark_generate_report(const char* filename, const BenchmarkResult* result) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("无法创建报告文件: %s\n", filename);
        return;
    }
    
    fprintf(file, "# Modyn 性能测试报告\n\n");
    
    // 测试配置
    fprintf(file, "## 测试配置\n");
    fprintf(file, "- 模型: %s\n", result->model_path ? result->model_path : "Unknown");
    fprintf(file, "- 后端: %s\n", result->backend_name ? result->backend_name : "Unknown");
    fprintf(file, "- 线程数: %d\n", result->thread_count);
    fprintf(file, "- 迭代次数: %d\n", result->iterations);
    fprintf(file, "- 预热次数: %d\n", result->warmup_iterations);
    fprintf(file, "- 使用内存池: %s\n", result->use_memory_pool ? "是" : "否");
    fprintf(file, "\n");
    
    // 测试结果
    fprintf(file, "## 测试结果\n");
    fprintf(file, "- 总成功次数: %d\n", result->total_success);
    fprintf(file, "- 总错误次数: %d\n", result->total_errors);
    fprintf(file, "- 成功率: %.2f%%\n", result->success_rate);
    fprintf(file, "- 最小延迟: %.2f ms\n", result->min_latency);
    fprintf(file, "- 最大延迟: %.2f ms\n", result->max_latency);
    fprintf(file, "- 平均延迟: %.2f ms\n", result->avg_latency);
    fprintf(file, "- 中位数延迟: %.2f ms\n", result->median_latency);
    fprintf(file, "- 99%分位延迟: %.2f ms\n", result->p99_latency);
    fprintf(file, "- 总耗时: %.2f s\n", result->total_time);
    fprintf(file, "- 总吞吐量: %.2f infer/s\n", result->total_throughput);
    fprintf(file, "- 每线程平均吞吐量: %.2f infer/s\n", result->avg_throughput_per_thread);
    fprintf(file, "\n");
    
    // 系统资源
    if (result->max_memory_mb > 0) {
        fprintf(file, "## 系统资源\n");
        fprintf(file, "- 最大内存使用: %.2f MB\n", result->max_memory_mb);
        fprintf(file, "- 平均CPU使用率: %.1f%%\n", result->avg_cpu_usage);
        fprintf(file, "\n");
    }
    
    // 时间戳
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(file, "## 测试信息\n");
    fprintf(file, "- 测试时间: %s\n", time_str);
    fprintf(file, "- 生成工具: Modyn Benchmark Tool v1.0.0\n");
    
    fclose(file);
    printf("性能报告已保存到: %s\n", filename);
}

void benchmark_calculate_percentiles(double* latencies, int count, 
                                    double* median, double* p95, double* p99) {
    // 简单的选择排序（对于大数据集应使用更高效的排序算法）
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (latencies[i] > latencies[j]) {
                double temp = latencies[i];
                latencies[i] = latencies[j];
                latencies[j] = temp;
            }
        }
    }
    
    // 计算百分位数
    if (median) {
        int median_idx = count / 2;
        *median = (count % 2 == 0) ? 
                  (latencies[median_idx - 1] + latencies[median_idx]) / 2.0 :
                  latencies[median_idx];
    }
    
    if (p95) {
        int p95_idx = (int)(count * 0.95);
        if (p95_idx >= count) p95_idx = count - 1;
        *p95 = latencies[p95_idx];
    }
    
    if (p99) {
        int p99_idx = (int)(count * 0.99);
        if (p99_idx >= count) p99_idx = count - 1;
        *p99 = latencies[p99_idx];
    }
}

void benchmark_print_progress(int current, int total, double elapsed_time) {
    if (total <= 0) return;
    
    double progress = (double)current / total;
    int bar_width = 50;
    int filled = (int)(progress * bar_width);
    
    printf("\r进度: [");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("=");
        } else if (i == filled) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    
    double estimated_total = (current > 0) ? elapsed_time / current * total : 0;
    double remaining = estimated_total - elapsed_time;
    
    printf("] %d/%d (%.1f%%) 已用时: %.1fs 剩余: %.1fs",
           current, total, progress * 100.0, elapsed_time / 1000.0, remaining / 1000.0);
    
    fflush(stdout);
    
    if (current == total) {
        printf("\n");
    }
} 