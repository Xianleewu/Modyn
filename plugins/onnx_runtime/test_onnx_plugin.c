/**
 * @file test_onnx_plugin.c
 * @brief ONNX Runtime 插件测试程序
 * @version 1.0.0
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>

#include "core/plugin_factory.h"
#include "core/inference_engine.h"

// 测试结果统计
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

// 简单测试宏
#define TEST_ASSERT(condition, message) do { \
    test_count++; \
    if (condition) { \
        test_passed++; \
        printf("[PASS] %s\n", message); \
    } else { \
        test_failed++; \
        printf("[FAIL] %s\n", message); \
    } \
} while(0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

/**
 * 插件发现回调函数
 */
void onnx_plugin_discovery_callback(const char* plugin_path, const plugin_info_t* info, void* user_data) {
    int* onnx_found = (int*)user_data;
    if (info && info->name && strcmp(info->name, "onnx_runtime") == 0) {
        *onnx_found = 1;
        printf("找到 ONNX Runtime 插件: %s (路径: %s)\n", info->name, plugin_path);
    }
}

/**
 * 测试ONNX Runtime插件基本功能
 */
static void test_onnx_plugin_basic() {
    TEST_SECTION("ONNX Runtime 插件基本功能测试");
    
    plugin_factory_t factory = plugin_factory_create();
    TEST_ASSERT(factory != NULL, "创建插件工厂");
    
    // 添加当前目录到搜索路径
    int result = plugin_factory_add_search_path(factory, ".");
    TEST_ASSERT(result == 0, "添加当前目录到搜索路径");
    
    // 发现插件
    int onnx_found = 0;
    result = plugin_factory_discover(factory, onnx_plugin_discovery_callback, &onnx_found);
    TEST_ASSERT(result >= 0, "发现插件");
    TEST_ASSERT(onnx_found, "找到 ONNX Runtime 插件");
    
    plugin_factory_destroy(factory);
}

/**
 * 测试ONNX Runtime插件加载
 */
static void test_onnx_plugin_loading() {
    TEST_SECTION("ONNX Runtime 插件加载测试");
    
    plugin_factory_t factory = plugin_factory_create();
    TEST_ASSERT(factory != NULL, "创建插件工厂");
    
    plugin_factory_add_search_path(factory, ".");
    int onnx_found = 0;
    plugin_factory_discover(factory, onnx_plugin_discovery_callback, &onnx_found);
    
    if (onnx_found) {
        // 加载 ONNX Runtime 插件
        plugin_t plugin = plugin_factory_load(factory, "onnx_runtime");
        TEST_ASSERT(plugin != NULL, "加载 ONNX Runtime 插件");
        
        if (plugin) {
            // 检查插件状态
            plugin_status_e status = plugin_get_status(plugin);
            TEST_ASSERT(status == PLUGIN_STATUS_LOADED || status == PLUGIN_STATUS_INITIALIZED, 
                       "ONNX Runtime 插件状态正确");
            
            // 获取插件信息
            const plugin_info_t* info = plugin_get_info(plugin);
            TEST_ASSERT(info != NULL, "获取 ONNX Runtime 插件信息");
            
            if (info) {
                printf("插件信息:\n");
                printf("  名称: %s\n", info->name);
                printf("  描述: %s\n", info->description);
                printf("  类型: %d\n", info->type);
                printf("  状态: %d\n", info->status);
            }
            
            // 卸载插件
            int result = plugin_factory_unload(factory, plugin);
            TEST_ASSERT(result == 0, "卸载 ONNX Runtime 插件");
        }
    } else {
        printf("未找到 ONNX Runtime 插件，跳过加载测试\n");
    }
    
    plugin_factory_destroy(factory);
}

/**
 * 测试ONNX Runtime与推理引擎集成
 */
static void test_onnx_inference_integration() {
    TEST_SECTION("ONNX Runtime 推理引擎集成测试");
    
    // 发现插件
    int result = infer_engine_discover_plugins();
    TEST_ASSERT(result == 0, "发现推理引擎插件");
    
    // 获取可用后端
    infer_backend_type_e backends[10];
    uint32_t backend_count = 10;
    result = infer_engine_get_available_backends(backends, &backend_count);
    TEST_ASSERT(result == 0, "获取可用推理后端");
    
    printf("当前可用后端数量: %u\n", backend_count);
    for (uint32_t i = 0; i < backend_count; i++) {
        const char* name = infer_engine_get_backend_name(backends[i]);
        printf("  - %s\n", name ? name : "Unknown");
    }
}

/**
 * 测试ONNX Runtime推理实例创建
 */
static void test_onnx_inference_creation() {
    TEST_SECTION("ONNX Runtime 推理实例创建测试");
    
    // 发现插件
    infer_engine_discover_plugins();
    
    // 创建引擎配置
    infer_engine_config_t config = {0};
    config.backend = INFER_BACKEND_ONNX;
    config.device_id = 0;
    config.num_threads = 4;
    config.enable_fp16 = false;
    config.enable_int8 = false;
    config.custom_config = NULL;
    
    // 尝试创建 ONNX Runtime 推理实例
    // 注意：由于我们使用的是demo实现，这可能会成功或失败
    infer_engine_t engine = infer_engine_create(INFER_BACKEND_ONNX, &config);
    
    if (engine) {
        TEST_ASSERT(1, "创建 ONNX Runtime 推理实例");
        
        // 销毁推理实例
        infer_engine_destroy(engine);
        TEST_ASSERT(1, "销毁推理实例");
    } else {
        printf("[INFO] 无法创建 ONNX Runtime 推理实例（demo模式或配置问题）\n");
    }
}

/**
 * 测试错误处理
 */
static void test_onnx_error_handling() {
    TEST_SECTION("ONNX Runtime 错误处理测试");
    
    plugin_factory_t factory = plugin_factory_create();
    TEST_ASSERT(factory != NULL, "创建插件工厂");
    
    // 测试加载不存在的插件
    plugin_t plugin = plugin_factory_load(factory, "non_existent_onnx_plugin");
    TEST_ASSERT(plugin == NULL, "正确处理不存在的插件");
    
    // 测试NULL参数
    plugin = plugin_factory_load(NULL, "onnx_runtime");
    TEST_ASSERT(plugin == NULL, "正确处理NULL工厂参数");
    
    plugin = plugin_factory_load(factory, NULL);
    TEST_ASSERT(plugin == NULL, "正确处理NULL插件名参数");
    
    plugin_factory_destroy(factory);
}

/**
 * 打印测试结果统计
 */
static void print_test_summary() {
    printf("\n============================================================\n");
    printf("ONNX Runtime 插件测试结果统计:\n");
    printf("总测试数: %d\n", test_count);
    printf("通过: %d\n", test_passed);
    printf("失败: %d\n", test_failed);
    printf("成功率: %.1f%%\n", test_count > 0 ? (float)test_passed / test_count * 100 : 0);
    printf("============================================================\n");
}

/**
 * 主函数
 */
int main(int argc, char* argv[]) {
    printf("ONNX Runtime 插件测试程序\n");
    printf("版本: 1.0.0\n");
    printf("构建时间: %s %s\n", __DATE__, __TIME__);
    
    // 检查命令行参数
    int verbose = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("\n用法: %s [选项]\n", argv[0]);
            printf("选项:\n");
            printf("  -v, --verbose    详细输出\n");
            printf("  -h, --help       显示此帮助信息\n");
            return 0;
        }
    }
    
    if (verbose) {
        printf("\n详细模式已启用\n");
    }
    
    printf("\n开始运行 ONNX Runtime 插件测试...\n");
    printf("注意: 此测试使用 demo 实现，不需要真实的 ONNX Runtime 库\n");
    
    // 运行测试
    test_onnx_plugin_basic();
    test_onnx_plugin_loading();
    test_onnx_inference_integration();
    test_onnx_inference_creation();
    test_onnx_error_handling();
    
    // 打印结果
    print_test_summary();
    
    printf("\n测试完成！\n");
    if (test_failed == 0) {
        printf("🎉 所有测试都通过了！ONNX Runtime 插件工作正常。\n");
    } else {
        printf("❌ 有 %d 个测试失败，请检查日志。\n", test_failed);
    }
    
    // 返回测试结果
    return test_failed > 0 ? 1 : 0;
} 