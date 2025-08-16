#ifndef MODYN_UTILS_LOGGER_H
#define MODYN_UTILS_LOGGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 日志级别枚举
 */
typedef enum {
    LOG_LEVEL_TRACE = 0,    /**< 跟踪信息 */
    LOG_LEVEL_DEBUG,        /**< 调试信息 */
    LOG_LEVEL_INFO,         /**< 一般信息 */
    LOG_LEVEL_WARN,         /**< 警告信息 */
    LOG_LEVEL_ERROR,        /**< 错误信息 */
    LOG_LEVEL_FATAL         /**< 致命错误 */
} log_level_e;

// 为了向后兼容，保留旧的类型别名
typedef log_level_e LogLevel;

/**
 * @brief 初始化日志系统
 * 
 * @param level 日志级别
 * @param log_file_path 日志文件路径，NULL表示不写文件
 */
void logger_init(LogLevel level, const char* log_file_path);

/**
 * @brief 清理日志系统
 */
void logger_cleanup(void);

/**
 * @brief 设置日志级别
 * 
 * @param level 日志级别
 */
void logger_set_level(LogLevel level);

/**
 * @brief 获取当前日志级别
 * 
 * @return LogLevel 当前日志级别
 */
LogLevel logger_get_level(void);

/**
 * @brief 设置是否输出到控制台
 * 
 * @param enable 是否启用
 */
void logger_set_console_output(bool enable);

/**
 * @brief 设置是否包含时间戳
 * 
 * @param enable 是否启用
 */
void logger_set_timestamp(bool enable);

/**
 * @brief 设置是否包含线程ID
 * 
 * @param enable 是否启用
 */
void logger_set_thread_id(bool enable);

/**
 * @brief 写入日志
 * 
 * @param level 日志级别
 * @param file 文件名
 * @param line 行号
 * @param func 函数名
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void logger_log(LogLevel level, const char* file, int line, const char* func, const char* format, ...);

// 便捷宏定义
#define LOG_TRACE(format, ...) logger_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  logger_log(LOG_LEVEL_INFO,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  logger_log(LOG_LEVEL_WARN,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // MODYN_UTILS_LOGGER_H 