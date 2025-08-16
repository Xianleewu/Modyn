# ONNX Runtime Plugin
cmake_minimum_required(VERSION 3.10)
project(modyn_onnx_runtime_plugin VERSION 1.0.0 LANGUAGES C)

# 设置 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找 ONNX Runtime (可选)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ONNXRUNTIME QUIET onnxruntime)
endif()

# 如果找到了真正的 ONNX Runtime，启用完整实现
if(ONNXRUNTIME_FOUND)
    add_definitions(-DUSE_REAL_ONNXRUNTIME)
    message(STATUS "Found ONNX Runtime, enabling full implementation")
else()
    message(STATUS "ONNX Runtime not found, using demo implementation")
endif()

# 插件源文件
set(PLUGIN_SOURCES
    onnx_runtime_plugin.c
)

# 创建插件动态库
add_library(onnx_runtime SHARED ${PLUGIN_SOURCES})

# 设置插件属性
set_target_properties(onnx_runtime PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PREFIX "lib"
    SUFFIX ".so"
)

# 包含目录
target_include_directories(onnx_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/utils ${CMAKE_SOURCE_DIR}/api ${CMAKE_SOURCE_DIR}/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(onnx_runtime
    pthread
    dl
)

# 如果找到了 ONNX Runtime，链接它
if(ONNXRUNTIME_FOUND)
    target_include_directories(onnx_runtime PRIVATE ${ONNXRUNTIME_INCLUDE_DIRS})
    target_link_libraries(onnx_runtime ${ONNXRUNTIME_LIBRARIES})
    target_compile_options(onnx_runtime PRIVATE ${ONNXRUNTIME_CFLAGS_OTHER})
endif()

# 编译选项
target_compile_options(onnx_runtime PRIVATE
    -Wall
    -Wextra
    -fPIC
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(onnx_runtime PRIVATE -g -DDEBUG)
endif()

# 插件信息文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/plugin.json"
    @ONLY
)

# 安装规则
install(TARGETS onnx_runtime
    LIBRARY DESTINATION plugins
    COMPONENT plugins
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/plugin.json"
    DESTINATION plugins
    COMPONENT plugins
)

# 创建插件包
set(CPACK_COMPONENT_PLUGINS_DISPLAY_NAME "ONNX Runtime Plugin")
set(CPACK_COMPONENT_PLUGINS_DESCRIPTION "ONNX Runtime inference engine plugin for Modyn")

# 测试
if(ENABLE_TESTS)
    # 插件测试
    add_executable(test_onnx_plugin test_onnx_plugin.c)
    target_include_directories(test_onnx_plugin PRIVATE
        ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/utils ${CMAKE_SOURCE_DIR}/api ${CMAKE_SOURCE_DIR}/pipeline
    )
    target_link_libraries(test_onnx_plugin
        modyn_core
        dl
        pthread
    )
    
    # 添加测试
    enable_testing()
    add_test(NAME onnx_plugin_test 
             COMMAND test_onnx_plugin $<TARGET_FILE:onnx_runtime>)
    set_tests_properties(onnx_plugin_test PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}"
    )
endif()

# 显示配置信息
message(STATUS "ONNX Runtime Plugin Configuration:")
message(STATUS "  Plugin Version: ${PROJECT_VERSION}")
message(STATUS "  ONNX Runtime Found: ${ONNXRUNTIME_FOUND}")
if(ONNXRUNTIME_FOUND)
    message(STATUS "  ONNX Runtime Libraries: ${ONNXRUNTIME_LIBRARIES}")
    message(STATUS "  ONNX Runtime Include Dirs: ${ONNXRUNTIME_INCLUDE_DIRS}")
endif()
message(STATUS "  Install Destination: ${CMAKE_INSTALL_PREFIX}/plugins") 