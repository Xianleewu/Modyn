#include "api/rest_server.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

/**
 * @brief REST API 服务器实现
 * 
 * 注意：这是一个简化的HTTP服务器实现，仅用于演示
 * 生产环境建议使用专业的HTTP库如libmicrohttpd或civetweb
 */

struct RestServer {
    char* host;
    uint16_t port;
    int server_fd;
    bool running;
    pthread_t server_thread;
    ModelManager* model_manager;
    pthread_mutex_t mutex;
};

// HTTP响应模板
static const char* HTTP_200_JSON = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Connection: close\r\n"
    "Content-Length: %zu\r\n"
    "\r\n"
    "%s";

static const char* HTTP_404 = 
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n"
    "Content-Length: 13\r\n"
    "\r\n"
    "404 Not Found";

static const char* HTTP_400 = 
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n"
    "Content-Length: 15\r\n"
    "\r\n"
    "400 Bad Request";

static const char* HTTP_500 = 
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n"
    "Content-Length: 25\r\n"
    "\r\n"
    "500 Internal Server Error";

// 解析HTTP请求
typedef struct {
    char method[16];
    char path[256];
    char body[4096];
    size_t body_length;
} HttpRequest;

static int parse_http_request(const char* request_data, HttpRequest* request) {
    if (!request_data || !request) {
        return -1;
    }
    
    // 解析请求行
    char* line_end = strstr(request_data, "\r\n");
    if (!line_end) {
        return -1;
    }
    
    sscanf(request_data, "%15s %255s", request->method, request->path);
    
    // 查找请求体
    char* body_start = strstr(request_data, "\r\n\r\n");
    if (body_start) {
        body_start += 4;  // 跳过 \r\n\r\n
        size_t body_len = strlen(body_start);
        if (body_len < sizeof(request->body)) {
            strcpy(request->body, body_start);
            request->body_length = body_len;
        }
    } else {
        request->body[0] = '\0';
        request->body_length = 0;
    }
    
    return 0;
}

// API处理函数
static void handle_health_check(int client_fd) {
    const char* response_body = "{\"status\":\"healthy\",\"service\":\"modyn\"}";
    char response[1024];
    
    snprintf(response, sizeof(response), HTTP_200_JSON, strlen(response_body), response_body);
    send(client_fd, response, strlen(response), 0);
    
    LOG_DEBUG("处理健康检查请求");
}

static void handle_models_list(int client_fd, ModelManager* manager) {
    if (!manager) {
        send(client_fd, HTTP_500, strlen(HTTP_500), 0);
        return;
    }
    
    // 简化实现：返回固定的模型列表
    const char* response_body = 
        "{"
        "\"models\":["
        "{\"id\":\"dummy_model\",\"status\":\"loaded\",\"backend\":\"dummy\"},"
        "{\"id\":\"test_model\",\"status\":\"unloaded\",\"backend\":\"dummy\"}"
        "],"
        "\"count\":2"
        "}";
    
    char response[2048];
    snprintf(response, sizeof(response), HTTP_200_JSON, strlen(response_body), response_body);
    send(client_fd, response, strlen(response), 0);
    
    LOG_DEBUG("处理模型列表请求");
}

static void handle_model_load(int client_fd, ModelManager* manager, const char* body) {
    if (!manager || !body) {
        send(client_fd, HTTP_400, strlen(HTTP_400), 0);
        return;
    }
    
    // 简化的JSON解析（生产环境应使用JSON库）
    char model_path[256] = {0};
    char model_id[256] = {0};
    
    // 查找model_path和model_id
    char* path_start = strstr(body, "\"model_path\":");
    char* id_start = strstr(body, "\"model_id\":");
    
    if (path_start && id_start) {
        sscanf(path_start, "\"model_path\":\"%255[^\"]\"", model_path);
        sscanf(id_start, "\"model_id\":\"%255[^\"]\"", model_id);
        
        // 尝试加载模型
        ModelConfig config = {0};
        config.model_path = model_path;
        config.model_id = model_id;
        config.version = "1.0";
        config.backend = INFER_BACKEND_DUMMY;
        
        ModelHandle model = model_manager_load(manager, model_path, &config);
        
        if (model) {
            char response_body[512];
            snprintf(response_body, sizeof(response_body),
                    "{\"status\":\"success\",\"message\":\"Model loaded\",\"model_id\":\"%s\"}", 
                    model_id);
            
            char response[1024];
            snprintf(response, sizeof(response), HTTP_200_JSON, strlen(response_body), response_body);
            send(client_fd, response, strlen(response), 0);
            
            LOG_INFO("通过API加载模型成功: %s", model_id);
        } else {
            const char* error_body = "{\"status\":\"error\",\"message\":\"Failed to load model\"}";
            char response[1024];
            snprintf(response, sizeof(response), HTTP_200_JSON, strlen(error_body), error_body);
            send(client_fd, response, strlen(response), 0);
            
            LOG_ERROR("通过API加载模型失败: %s", model_id);
        }
    } else {
        send(client_fd, HTTP_400, strlen(HTTP_400), 0);
    }
}

static void handle_model_infer(int client_fd, ModelManager* manager, const char* model_id, const char* body) {
    if (!manager || !model_id || !body) {
        send(client_fd, HTTP_400, strlen(HTTP_400), 0);
        return;
    }
    
    ModelHandle model = model_manager_get(manager, model_id);
    if (!model) {
        const char* error_body = "{\"status\":\"error\",\"message\":\"Model not found\"}";
        char response[1024];
        snprintf(response, sizeof(response), HTTP_200_JSON, strlen(error_body), error_body);
        send(client_fd, response, strlen(response), 0);
        return;
    }
    
    // 简化实现：返回虚拟推理结果
    const char* response_body = 
        "{"
        "\"status\":\"success\","
        "\"model_id\":\"%s\","
        "\"result\":{"
        "\"confidence\":0.8765,"
        "\"prediction\":42,"
        "\"latency_ms\":15.2"
        "}"
        "}";
    
    char full_response_body[1024];
    snprintf(full_response_body, sizeof(full_response_body), response_body, model_id);
    
    char response[2048];
    snprintf(response, sizeof(response), HTTP_200_JSON, strlen(full_response_body), full_response_body);
    send(client_fd, response, strlen(response), 0);
    
    // 释放模型句柄
    model_manager_unload(manager, model);
    
    LOG_DEBUG("处理推理请求: %s", model_id);
}

// 处理客户端连接
static void handle_client(int client_fd, RestServer* server) {
    char buffer[8192];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    HttpRequest request;
    if (parse_http_request(buffer, &request) != 0) {
        send(client_fd, HTTP_400, strlen(HTTP_400), 0);
        close(client_fd);
        return;
    }
    
    LOG_DEBUG("收到请求: %s %s", request.method, request.path);
    
    // 路由处理
    if (strcmp(request.method, "GET") == 0) {
        if (strcmp(request.path, "/health") == 0) {
            handle_health_check(client_fd);
        } else if (strcmp(request.path, "/models") == 0) {
            handle_models_list(client_fd, server->model_manager);
        } else {
            send(client_fd, HTTP_404, strlen(HTTP_404), 0);
        }
    } else if (strcmp(request.method, "POST") == 0) {
        if (strcmp(request.path, "/models") == 0) {
            handle_model_load(client_fd, server->model_manager, request.body);
        } else if (strncmp(request.path, "/models/", 8) == 0) {
            char* model_id = request.path + 8;
            char* infer_pos = strstr(model_id, "/infer");
            if (infer_pos) {
                *infer_pos = '\0';  // 截断获取model_id
                handle_model_infer(client_fd, server->model_manager, model_id, request.body);
            } else {
                send(client_fd, HTTP_404, strlen(HTTP_404), 0);
            }
        } else {
            send(client_fd, HTTP_404, strlen(HTTP_404), 0);
        }
    } else {
        send(client_fd, HTTP_400, strlen(HTTP_400), 0);
    }
    
    close(client_fd);
}

// 服务器线程函数
static void* server_thread_func(void* arg) {
    RestServer* server = (RestServer*)arg;
    
    LOG_INFO("REST API服务器开始监听 %s:%d", server->host, server->port);
    
    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (server->running) {
                LOG_ERROR("接受连接失败");
            }
            continue;
        }
        
        // 简化实现：同步处理（生产环境应使用线程池）
        handle_client(client_fd, server);
    }
    
    LOG_INFO("REST API服务器线程退出");
    return NULL;
}

RestServer* rest_server_create(const char* host, uint16_t port, ModelManager* model_manager) {
    if (!host || !model_manager) {
        LOG_ERROR("REST服务器创建参数无效");
        return NULL;
    }
    
    RestServer* server = malloc(sizeof(RestServer));
    if (!server) {
        LOG_ERROR("REST服务器内存分配失败");
        return NULL;
    }
    
    server->host = strdup(host);
    server->port = port;
    server->running = false;
    server->model_manager = model_manager;
    
    if (pthread_mutex_init(&server->mutex, NULL) != 0) {
        LOG_ERROR("REST服务器互斥锁初始化失败");
        free(server->host);
        free(server);
        return NULL;
    }
    
    LOG_INFO("REST API服务器创建成功: %s:%d", host, port);
    return server;
}

void rest_server_destroy(RestServer* server) {
    if (!server) {
        return;
    }
    
    rest_server_stop(server);
    
    pthread_mutex_destroy(&server->mutex);
    free(server->host);
    free(server);
    
    LOG_INFO("REST API服务器销毁完成");
}

int rest_server_start(RestServer* server) {
    if (!server || server->running) {
        return -1;
    }
    
    // 创建socket
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        LOG_ERROR("创建socket失败");
        return -1;
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARN("设置SO_REUSEADDR失败");
    }
    
    // 绑定地址
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    
    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("绑定地址失败: %s:%d", server->host, server->port);
        close(server->server_fd);
        return -1;
    }
    
    // 开始监听
    if (listen(server->server_fd, 10) < 0) {
        LOG_ERROR("开始监听失败");
        close(server->server_fd);
        return -1;
    }
    
    // 启动服务器线程
    server->running = true;
    if (pthread_create(&server->server_thread, NULL, server_thread_func, server) != 0) {
        LOG_ERROR("创建服务器线程失败");
        server->running = false;
        close(server->server_fd);
        return -1;
    }
    
    LOG_INFO("REST API服务器启动成功: http://%s:%d", server->host, server->port);
    return 0;
}

int rest_server_stop(RestServer* server) {
    if (!server || !server->running) {
        return -1;
    }
    
    server->running = false;
    
    // 关闭服务器socket
    if (server->server_fd >= 0) {
        close(server->server_fd);
        server->server_fd = -1;
    }
    
    // 等待服务器线程结束
    pthread_join(server->server_thread, NULL);
    
    LOG_INFO("REST API服务器停止");
    return 0;
}

bool rest_server_is_running(RestServer* server) {
    return server ? server->running : false;
} 