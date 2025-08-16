/**
 * @file plugin_test_runner.c
 * @brief 插件系统测试运行器
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
void plugin_discovery_callback(const char* plugin_path, const plugin_info_t* info, void* user_data) {
    int* count = (int*)user_data;
    if (info && info->name) {
        (*count)++;
        printf("发现插件: %s (路径: %s)\n", info->name, plugin_path);
    }
}

/**
 * 测试插件工厂基本功能
 */
static void test_plugin_factory_basic() {
    TEST_SECTION("插件工厂基本功能测试");
    
    plugin_factory_t factory = plugin_factory_create();
    TEST_ASSERT(factory != NULL, "创建插件工厂");
    
    // 测试添加搜索路径
    int result = plugin_factory_add_search_path(factory, "./plugins");
    TEST_ASSERT(result == 0, "添加搜索路径");
    
    // 测试发现插件
    int plugin_count = 0;
    result = plugin_factory_discover(factory, plugin_discovery_callback, &plugin_count);
    TEST_ASSERT(result >= 0, "发现插件");
    printf("发现插件数量: %d\n", plugin_count);
    
    plugin_factory_destroy(factory);
    TEST_ASSERT(1, "销毁插件工厂");
}

/**
 * 测试插件加载和卸载
 */
static void test_plugin_load_unload() {
    TEST_SECTION("插件加载和卸载测试");
    
    plugin_factory_t factory = plugin_factory_create();
    TEST_ASSERT(factory != NULL, "创建插件工厂");
    
    plugin_factory_add_search_path(factory, "./plugins");
    int plugin_count = 0;
    plugin_factory_discover(factory, plugin_discovery_callback, &plugin_count);
    
    if (plugin_count > 0) {
        // 尝试加载一个插件
        plugin_t plugin = plugin_factory_load(factory, "onnx_runtime");
        TEST_ASSERT(plugin != NULL, "加载插件");
        
        if (plugin) {
            // 检查插件状态
            plugin_status_e status = plugin_get_status(plugin);
            TEST_ASSERT(status == PLUGIN_STATUS_LOADED || status == PLUGIN_STATUS_INITIALIZED, 
                       "检查插件状态");
            
            // 卸载插件
            int result = plugin_factory_unload(factory, plugin);
            TEST_ASSERT(result == 0, "卸载插件");
        }
    } else {
        printf("未发现插件，跳过加载测试\n");
    }
    
    plugin_factory_destroy(factory);
}

/**
 * 测试推理引擎插件集成
 */
static void test_inference_engine_integration() {
    TEST_SECTION("推理引擎插件集成测试");
    
    // 发现插件
    int result = infer_engine_discover_plugins();
    TEST_ASSERT(result == 0, "发现推理引擎插件");
    
    // 获取可用后端
    infer_backend_type_e backends[10];
    uint32_t backend_count = 10;
    result = infer_engine_get_available_backends(backends, &backend_count);
    TEST_ASSERT(result == 0 && backend_count > 0, "获取可用推理后端");
    
    printf("可用推理后端数量: %u\n", backend_count);
    for (uint32_t i = 0; i < backend_count; i++) {
        const char* name = infer_engine_get_backend_name(backends[i]);
        printf("  - %s\n", name ? name : "Unknown");
    }
}

/**
 * 测试版本比较功能
 */
static void test_version_comparison() {
    TEST_SECTION("版本比较功能测试");
    
    plugin_version_t v1 = {1, 0, 0, NULL};
    plugin_version_t v2 = {1, 0, 0, NULL};
    plugin_version_t v3 = {1, 0, 1, NULL};
    
    TEST_ASSERT(plugin_version_compare(&v1, &v2) == 0, "相同版本比较");
    TEST_ASSERT(plugin_version_compare(&v3, &v1) > 0, "新版本大于旧版本");
    TEST_ASSERT(plugin_version_compare(&v1, &v3) < 0, "旧版本小于新版本");
}

/**
 * 测试错误处理
 */
static void test_error_handling() {
    TEST_SECTION("错误处理测试");
    
    plugin_factory_t factory = plugin_factory_create();
    TEST_ASSERT(factory != NULL, "创建插件工厂用于错误测试");
    
    // 测试加载不存在的插件
    plugin_t plugin = plugin_factory_load(factory, "non_existent_plugin");
    TEST_ASSERT(plugin == NULL, "加载不存在插件应该失败");
    
    // 测试添加无效路径
    int result = plugin_factory_add_search_path(factory, "/invalid/path/that/does/not/exist");
    TEST_ASSERT(result != 0, "添加无效路径应该失败");
    
    // 测试传入NULL参数
    plugin_t null_plugin = plugin_factory_load(NULL, "test");
    TEST_ASSERT(null_plugin == NULL, "传入NULL工厂应该返回NULL");
    
    null_plugin = plugin_factory_load(factory, NULL);
    TEST_ASSERT(null_plugin == NULL, "传入NULL插件名应该返回NULL");
    
    plugin_factory_destroy(factory);
}

/**
 * 打印测试结果统计
 */
static void print_test_summary() {
    printf("\n==================================================\n");
    printf("测试结果统计:\n");
    printf("总测试数: %d\n", test_count);
    printf("通过: %d\n", test_passed);
    printf("失败: %d\n", test_failed);
    printf("成功率: %.1f%%\n", test_count > 0 ? (float)test_passed / test_count * 100 : 0);
    printf("==================================================\n");
}

/**
 * 主函数
 */
int main(int argc, char* argv[]) {
    printf("Modyn 插件系统测试运行器\n");
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
    
    printf("\n开始运行插件系统测试...\n");
    
    // 运行测试
    test_plugin_factory_basic();
    test_plugin_load_unload();
    test_inference_engine_integration();
    test_version_comparison();
    test_error_handling();
    
    // 打印结果
    print_test_summary();
    
    // 返回测试结果
    return test_failed > 0 ? 1 : 0;
} 