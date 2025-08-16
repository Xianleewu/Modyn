#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

static LogLevel current_level = LOG_LEVEL_INFO;
static bool console_output = true;
static FILE* log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char* level_strings[] = {
    "TRACE",
    "DEBUG", 
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

static const char* level_colors[] = {
    "\033[37m",  // TRACE - white
    "\033[36m",  // DEBUG - cyan
    "\033[32m",  // INFO - green
    "\033[33m",  // WARN - yellow
    "\033[31m",  // ERROR - red
    "\033[35m"   // FATAL - magenta
};

void logger_init(LogLevel level, const char* log_file_path) {
    current_level = level;
    if (log_file_path) {
        logger_set_file_output(log_file_path);
    }
}

void logger_set_level(LogLevel level) {
    current_level = level;
}

LogLevel logger_get_level(void) {
    return current_level;
}

void logger_set_console_output(bool enable) {
    console_output = enable;
}

void logger_set_timestamp(bool enable) {
    // 简化实现，时间戳总是启用
    (void)enable;
}

void logger_set_thread_id(bool enable) {
    // 简化实现，线程ID暂不支持
    (void)enable;
}

void logger_set_file_output(const char* filename) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file && log_file != stdout && log_file != stderr) {
        fclose(log_file);
    }
    
    if (filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", filename);
            log_file = NULL;
        }
    } else {
        log_file = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void logger_log(LogLevel level, const char* file, int line, const char* func, const char* format, ...) {
    if (level < current_level) {
        return;
    }
    
    pthread_mutex_lock(&log_mutex);
    
    // 获取当前时间
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    // 格式化消息
    va_list args;
    va_start(args, format);
    
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    
    va_end(args);
    
    // 输出到控制台
    if (console_output) {
        fprintf(stderr, "%s[%s] %s (%s:%d %s) %s\033[0m\n",
                level_colors[level],
                level_strings[level],
                timestamp,
                file,
                line,
                func,
                message);
    }
    
    // 输出到文件
    if (log_file) {
        fprintf(log_file, "[%s] %s (%s:%d %s) %s\n",
                level_strings[level],
                timestamp,
                file,
                line,
                func,
                message);
        fflush(log_file);
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void logger_cleanup(void) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file && log_file != stdout && log_file != stderr) {
        fclose(log_file);
        log_file = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
} 