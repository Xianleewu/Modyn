#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include "api/rest_server.h"
#include "core/model_manager.h"
#include "utils/logger.h"

/**
 * @brief Modyn API 服务器主程序
 */

static bool server_running = true;
static RestServer* rest_server = NULL;

void signal_handler(int signum) {
    printf("\n收到信号 %d，正在关闭服务器...\n", signum);
    server_running = false;
    
    if (rest_server) {
        rest_server_stop(rest_server);
    }
}

void print_usage(const char* program_name) {
    printf("Modyn API 服务器\n");
    printf("\n");
    printf("用法: %s [选项]\n", program_name);
    printf("\n");
    printf("选项:\n");
    printf("  -h, --host <地址>       绑定地址 (默认: 0.0.0.0)\n");
    printf("  -p, --port <端口>       监听端口 (默认: 8080)\n");
    printf("  -l, --log <文件>        日志文件路径\n");
    printf("  -v, --verbose           详细输出\n");
    printf("  --help                  显示帮助信息\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s                      # 使用默认配置启动\n", program_name);
    printf("  %s -h 127.0.0.1 -p 9090 # 绑定到localhost:9090\n", program_name);
    printf("\n");
    printf("API 端点:\n");
    printf("  GET  /health            # 健康检查\n");
    printf("  GET  /models            # 列出所有模型\n");
    printf("  POST /models            # 加载模型\n");
    printf("  POST /models/{id}/infer # 执行推理\n");
    printf("\n");
}

void print_banner(void) {
    printf("  __  __           _             \n");
    printf(" |  \\/  | ___   __| |_   _ _ __  \n");
    printf(" | |\\/| |/ _ \\ / _` | | | | '_ \\ \n");
    printf(" | |  | | (_) | (_| | |_| | | | |\n");
    printf(" |_|  |_|\\___/ \\__,_|\\__, |_| |_|\n");
    printf("                    |___/       \n");
    printf("\n");
    printf("Modyn 跨平台模型推理服务系统 API 服务器\n");
    printf("版本: 1.0.0\n");
    printf("\n");
}

int main(int argc, char* argv[]) {
    // 默认配置
    char* host = "0.0.0.0";
    uint16_t port = 8080;
    char* log_file = NULL;
    LogLevel log_level = LOG_LEVEL_INFO;
    
    // 解析命令行参数
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"log", required_argument, 0, 'l'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'H'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "h:p:l:vH", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = (uint16_t)atoi(optarg);
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'v':
                log_level = LOG_LEVEL_DEBUG;
                break;
            case 'H':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }
    
    // 验证参数
    if (port == 0) {
        printf("❌ 无效的端口号\n");
        return 1;
    }
    
    // 显示启动信息
    print_banner();
    
    // 初始化日志系统
    logger_init(log_level, log_file);
    logger_set_console_output(true);
    logger_set_timestamp(true);
    
    LOG_INFO("启动 Modyn API 服务器");
    LOG_INFO("配置: %s:%d", host, port);
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 创建模型管理器
    LOG_INFO("初始化模型管理器...");
    ModelManager* manager = model_manager_create();
    if (!manager) {
        LOG_FATAL("模型管理器创建失败");
        return 1;
    }
    
    // 创建REST服务器
    LOG_INFO("创建REST API服务器...");
    rest_server = rest_server_create(host, port, manager);
    if (!rest_server) {
        LOG_FATAL("REST服务器创建失败");
        model_manager_destroy(manager);
        return 1;
    }
    
    // 启动服务器
    LOG_INFO("启动REST API服务器...");
    if (rest_server_start(rest_server) != 0) {
        LOG_FATAL("REST服务器启动失败");
        rest_server_destroy(rest_server);
        model_manager_destroy(manager);
        return 1;
    }
    
    printf("🚀 Modyn API 服务器启动成功！\n");
    printf("   访问地址: http://%s:%d\n", host, port);
    printf("   健康检查: http://%s:%d/health\n", host, port);
    printf("   按 Ctrl+C 停止服务器\n");
    printf("\n");
    
    // 主循环
    while (server_running && rest_server_is_running(rest_server)) {
        sleep(1);
    }
    
    // 清理资源
    LOG_INFO("清理资源...");
    
    if (rest_server) {
        rest_server_destroy(rest_server);
    }
    
    if (manager) {
        model_manager_destroy(manager);
    }
    
    LOG_INFO("Modyn API 服务器已停止");
    logger_cleanup();
    
    printf("\n👋 再见！\n");
    
    return 0;
} 