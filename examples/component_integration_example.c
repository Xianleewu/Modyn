#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "component_manager.h"

// ======================== 优雅的组件集成示例 ======================== //

// 示例：如何将现有的驱动代码优雅地集成到新的组件系统中
// 无需重写现有逻辑，只需添加适配器层

// 假设这是现有的驱动结构
typedef struct existing_driver_s {
    char name[64];
    char version[32];
    int device_type;
    void *ops;
    // ... 其他现有字段
} existing_driver_t;

// 现有的驱动操作函数
static int existing_driver_check_feature(const existing_driver_t *driver, const char *feature) {
    (void)driver;
    if (strcmp(feature, "feature_a") == 0) return 1;
    if (strcmp(feature, "feature_b") == 0) return 1;
    return 0;
}

static const char* existing_driver_get_info(const existing_driver_t *driver) {
    (void)driver;
    return "Existing driver with legacy interface";
}

// ======================== 优雅的适配器层 ======================== //

// 组件初始化适配器 - 复用现有逻辑
static modyn_status_e component_initialize_adapter(void *component, void *config) {
    existing_driver_t *driver = (existing_driver_t*)component;
    if (!driver) return MODYN_ERROR_INVALID_ARGUMENT;
    
    // 调用现有的初始化逻辑（如果有的话）
    // 或者直接返回成功，表示无需额外初始化
    printf("Component %s initialized (using existing logic)\n", driver->name);
    return MODYN_SUCCESS;
}

// 组件关闭适配器 - 复用现有逻辑
static modyn_status_e component_shutdown_adapter(void *component) {
    existing_driver_t *driver = (existing_driver_t*)component;
    if (!driver) return MODYN_ERROR_INVALID_ARGUMENT;
    
    // 调用现有的清理逻辑（如果有的话）
    printf("Component %s shutdown (using existing logic)\n", driver->name);
    return MODYN_SUCCESS;
}

// 组件状态查询适配器 - 复用现有逻辑
static modyn_component_status_e component_status_adapter(const void *component) {
    const existing_driver_t *driver = (const existing_driver_t*)component;
    if (!driver) return MODYN_COMPONENT_ERROR;
    
    // 基于现有驱动状态返回组件状态
    return MODYN_COMPONENT_ACTIVE;
}

// 组件查询适配器 - 复用现有逻辑
static modyn_status_e component_query_adapter(const void *component, void *query_info) {
    const existing_driver_t *driver = (const existing_driver_t*)component;
    if (!driver) return MODYN_ERROR_INVALID_ARGUMENT;
    
    // 调用现有的查询逻辑
    printf("Component query: %s\n", existing_driver_get_info(driver));
    return MODYN_SUCCESS;
}

// 组件能力检查适配器 - 复用现有逻辑
static int component_feature_adapter(const void *component, const char *feature) {
    const existing_driver_t *driver = (const existing_driver_t*)component;
    if (!driver || !feature) return 0;
    
    // 直接调用现有的能力检查函数
    return existing_driver_check_feature(driver, feature);
}

// 组件能力描述适配器 - 复用现有逻辑
static const char* component_capabilities_adapter(const void *component) {
    const existing_driver_t *driver = (const existing_driver_t*)component;
    if (!driver) return "Unknown";
    
    // 调用现有的信息获取函数
    return existing_driver_get_info(driver);
}

// ======================== 组件接口定义 ======================== //

// 定义组件接口 - 通过适配器复用现有功能
static modyn_component_interface_t elegant_component_interface = {
    .name = "elegant_example",
    .version = "1.0.0",
    .type = MODYN_COMPONENT_DEVICE,
    .source = MODYN_COMPONENT_SOURCE_BUILTIN,
    
    // 注意：生命周期管理方法已移除，由组件自己负责
    
    // 状态查询 - 通过适配器复用现有逻辑
    .get_status = component_status_adapter,
    .query = component_query_adapter,
    
    // 能力查询 - 通过适配器复用现有逻辑
    .supports_feature = component_feature_adapter,
    .get_capabilities = component_capabilities_adapter,
    
    // 私有数据 - 指向现有的驱动结构
    .private_data = NULL  // 在注册时设置
};

// ======================== 优雅的注册方式 ======================== //

// 创建并注册组件的优雅方式
static void register_elegant_component(existing_driver_t *existing_driver) {
    // 设置私有数据指向现有驱动
    elegant_component_interface.private_data = existing_driver;
    
    // 注册组件
    modyn_status_e status = modyn_register_component(
        MODYN_COMPONENT_DEVICE,
        existing_driver->name,
        &elegant_component_interface,
        MODYN_COMPONENT_SOURCE_BUILTIN
    );
    
    if (status == MODYN_SUCCESS) {
        printf("✓ Elegantly registered component: %s\n", existing_driver->name);
    } else {
        printf("✗ Failed to register component: %s (status: %d)\n", 
               existing_driver->name, status);
    }
}

// ======================== 使用示例 ======================== //

int main() {
    // 初始化组件管理器
    modyn_status_e status = modyn_component_manager_init(NULL);
    if (status != MODYN_SUCCESS) {
        printf("Failed to initialize component manager: %d\n", status);
        return -1;
    }
    
    // 创建现有的驱动实例
    existing_driver_t existing_driver = {
        .name = "legacy_driver",
        .version = "2.1.0",
        .device_type = 1,
        .ops = NULL
    };
    
    // 优雅地注册组件
    register_elegant_component(&existing_driver);
    
    // 查找并测试组件
    modyn_component_interface_t *found = modyn_find_component(
        MODYN_COMPONENT_DEVICE, "legacy_driver");
    
    if (found) {
        printf("✓ Found component: %s\n", found->name);
        
        // 测试组件功能
        if (found->query) {
            found->query(found->private_data, NULL);
        }
        
        if (found->supports_feature) {
            int supports = found->supports_feature(found->private_data, "feature_a");
            printf("Supports feature_a: %s\n", supports ? "Yes" : "No");
        }
    }
    
    // 清理
    modyn_component_manager_shutdown();
    return 0;
}

// ======================== 关键设计原则 ======================== //

/*
 * 优雅的组件集成设计原则：
 * 
 * 1. **最小侵入性**：不重写现有代码，只添加适配器层
 * 2. **功能复用**：通过适配器直接调用现有的功能函数
 * 3. **接口统一**：所有组件都实现相同的接口，便于管理
 * 4. **向后兼容**：保留现有的驱动结构，确保兼容性
 * 5. **灵活配置**：通过私有数据指针灵活传递现有结构
 * 
 * 优势：
 * - 无需重写现有逻辑
 * - 代码复用最大化
 * - 维护成本最低
 * - 扩展性最好
 * - 符合开闭原则
 */
