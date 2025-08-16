#ifndef MODYN_API_REST_SERVER_H
#define MODYN_API_REST_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "core/model_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief REST API 服务器句柄
 */
typedef struct RestServer RestServer;

/**
 * @brief 创建REST API服务器
 * 
 * @param host 绑定的主机地址
 * @param port 监听端口
 * @param model_manager 模型管理器
 * @return RestServer* 服务器指针，失败返回NULL
 */
RestServer* rest_server_create(const char* host, uint16_t port, ModelManager* model_manager);

/**
 * @brief 销毁REST API服务器
 * 
 * @param server 服务器指针
 */
void rest_server_destroy(RestServer* server);

/**
 * @brief 启动REST API服务器
 * 
 * @param server 服务器指针
 * @return int 0表示成功，负数表示失败
 */
int rest_server_start(RestServer* server);

/**
 * @brief 停止REST API服务器
 * 
 * @param server 服务器指针
 * @return int 0表示成功，负数表示失败
 */
int rest_server_stop(RestServer* server);

/**
 * @brief 检查服务器是否运行中
 * 
 * @param server 服务器指针
 * @return bool 运行中返回true
 */
bool rest_server_is_running(RestServer* server);

#ifdef __cplusplus
}
#endif

#endif // MODYN_API_REST_SERVER_H 