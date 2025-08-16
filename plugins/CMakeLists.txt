# Modyn Plugins
cmake_minimum_required(VERSION 3.10)

message(STATUS "Building Modyn Plugins...")

# 插件通用设置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 插件通用编译选项
function(setup_plugin_target target_name)
    # 设置为共享库
    set_target_properties(${target_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        PREFIX "lib"
        SUFFIX ".so"
    )
    
    # 包含目录
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # 链接基础库
    target_link_libraries(${target_name}
        pthread
        dl
    )
    
    # 编译选项
    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -fPIC
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${target_name} PRIVATE -g -DDEBUG)
    endif()
    
    # 安装规则
    install(TARGETS ${target_name}
        LIBRARY DESTINATION plugins
        COMPONENT plugins
    )
endfunction()

# ONNX Runtime 插件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/onnx_runtime/CMakeLists.txt")
    message(STATUS "  Adding ONNX Runtime plugin")
    add_subdirectory(onnx_runtime)
else()
    message(STATUS "  ONNX Runtime plugin not found, skipping")
endif()

# OpenVINO 插件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/openvino/CMakeLists.txt")
    message(STATUS "  Adding OpenVINO plugin")
    add_subdirectory(openvino)
else()
    message(STATUS "  OpenVINO plugin not found, skipping")
endif()

# TensorRT 插件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tensorrt/CMakeLists.txt")
    message(STATUS "  Adding TensorRT plugin")
    add_subdirectory(tensorrt)
else()
    message(STATUS "  TensorRT plugin not found, skipping")
endif()

# PyTorch 插件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pytorch/CMakeLists.txt")
    message(STATUS "  Adding PyTorch plugin")
    add_subdirectory(pytorch)
else()
    message(STATUS "  PyTorch plugin not found, skipping")
endif()

# TensorFlow 插件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tensorflow/CMakeLists.txt")
    message(STATUS "  Adding TensorFlow plugin")
    add_subdirectory(tensorflow)
else()
    message(STATUS "  TensorFlow plugin not found, skipping")
endif()

# 自定义插件目录扫描
file(GLOB PLUGIN_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*")
foreach(PLUGIN_DIR ${PLUGIN_DIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_DIR}")
        # 跳过已处理的插件
        if(NOT ${PLUGIN_DIR} STREQUAL "onnx_runtime" AND
           NOT ${PLUGIN_DIR} STREQUAL "openvino" AND
           NOT ${PLUGIN_DIR} STREQUAL "tensorrt" AND
           NOT ${PLUGIN_DIR} STREQUAL "pytorch" AND
           NOT ${PLUGIN_DIR} STREQUAL "tensorflow")
            
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_DIR}/CMakeLists.txt")
                message(STATUS "  Adding custom plugin: ${PLUGIN_DIR}")
                add_subdirectory(${PLUGIN_DIR})
            endif()
        endif()
    endif()
endforeach()

# 插件测试
if(ENABLE_TESTS)
    # 创建插件测试工具
    add_executable(plugin_test_runner plugin_test_runner.c)
    target_include_directories(plugin_test_runner PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(plugin_test_runner
        modyn_core
        dl
        pthread
    )
    
    # 将测试工具放在bin目录
    set_target_properties(plugin_test_runner PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # 安装测试工具
    install(TARGETS plugin_test_runner
        RUNTIME DESTINATION bin
        COMPONENT development
    )
    
    # 插件测试
    enable_testing()
    
    # 创建插件测试目录
    set(PLUGIN_TEST_DIR "${CMAKE_BINARY_DIR}/plugins")
    
    # 为每个编译的插件添加测试
    if(TARGET onnx_runtime)
        add_test(NAME test_onnx_runtime_plugin
                 COMMAND plugin_test_runner "${PLUGIN_TEST_DIR}/libonnx_runtime.so")
    endif()
    
    # 通用插件发现测试
    add_test(NAME test_plugin_discovery
             COMMAND plugin_test_runner --discover "${PLUGIN_TEST_DIR}")
             
    # 设置测试环境
    set_tests_properties(test_plugin_discovery PROPERTIES
        ENVIRONMENT "MODYN_PLUGIN_PATH=${PLUGIN_TEST_DIR}"
    )
endif()

# 插件文档
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        DESTINATION plugins
        RENAME "PluginREADME.md"
        COMPONENT documentation
    )
endif()

# 插件示例配置
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin_config_example.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/plugin_config_example.json"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/plugin_config_example.json"
    DESTINATION plugins
    COMPONENT documentation
)

message(STATUS "Plugins configuration complete") 