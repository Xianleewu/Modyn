cmake_minimum_required(VERSION 3.10)
project(modyn VERSION 1.0.0 LANGUAGES C CXX)

# 设置 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
option(ENABLE_RKNN "Enable RKNN backend" OFF)
option(ENABLE_OPENVINO "Enable OpenVINO backend" OFF)
option(ENABLE_TENSORRT "Enable TensorRT backend" OFF)
option(ENABLE_API "Enable REST/gRPC API" OFF)
option(ENABLE_OPENCV "Enable OpenCV support" OFF)
option(ENABLE_TESTS "Enable unit tests" ON)
option(ENABLE_PLUGINS "Enable plugin system" ON)
option(BUILD_PLUGINS "Build example plugins" ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录 - 各模块头文件与源文件在同一目录
include_directories(
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/utils  
    ${CMAKE_SOURCE_DIR}/api
    ${CMAKE_SOURCE_DIR}/pipeline
    ${CMAKE_SOURCE_DIR}
)

# 查找依赖包
find_package(PkgConfig REQUIRED)

# OpenCV 支持
if(ENABLE_OPENCV)
    find_package(OpenCV REQUIRED)
    add_definitions(-DMODYN_ENABLE_OPENCV)
endif()

# 线程支持
find_package(Threads REQUIRED)
find_package(${CMAKE_DL_LIBS} QUIET)

# 编译器选项
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -O2)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -DDEBUG)
    endif()
endif()

# 核心库源文件
set(CORE_SOURCES
    core/model_manager.c
    core/inference_engine.c
    core/tensor.c
    core/registry.c
    core/model_parser.c
    core/memory_pool.c
    core/multimodal.c
)

# 插件工厂源文件
if(ENABLE_PLUGINS)
    list(APPEND CORE_SOURCES core/plugin_factory.c)
    add_definitions(-DMODYN_ENABLE_PLUGINS)
endif()

set(UTILS_SOURCES
    utils/image_utils.c
    utils/logger.c
    utils/preprocessing.c
)

set(PIPELINE_SOURCES
    pipeline/pipeline_manager.c
)

# 创建核心库
add_library(modyn_core STATIC ${CORE_SOURCES} ${UTILS_SOURCES} ${PIPELINE_SOURCES})
target_link_libraries(modyn_core Threads::Threads)

# 链接动态库加载库
if(ENABLE_PLUGINS)
    target_link_libraries(modyn_core ${CMAKE_DL_LIBS})
endif()

# 后端支持
set(BACKEND_LIBS)

# RKNN 后端
if(ENABLE_RKNN)
    add_subdirectory(backend/rknn)
    list(APPEND BACKEND_LIBS modyn_rknn)
    add_definitions(-DMODYN_ENABLE_RKNN)
endif()

# OpenVINO 后端
if(ENABLE_OPENVINO)
    add_subdirectory(backend/openvino)
    list(APPEND BACKEND_LIBS modyn_openvino)
    add_definitions(-DMODYN_ENABLE_OPENVINO)
endif()

# TensorRT 后端
if(ENABLE_TENSORRT)
    add_subdirectory(backend/tensorrt)
    list(APPEND BACKEND_LIBS modyn_tensorrt)
    add_definitions(-DMODYN_ENABLE_TENSORRT)
endif()

# Dummy 后端（调试用）
add_subdirectory(backend/dummy)
list(APPEND BACKEND_LIBS modyn_dummy)

# 创建主库
add_library(modyn SHARED ${CORE_SOURCES} ${UTILS_SOURCES} ${PIPELINE_SOURCES})
target_link_libraries(modyn ${BACKEND_LIBS} Threads::Threads)

# 链接动态库加载库
if(ENABLE_PLUGINS)
    target_link_libraries(modyn ${CMAKE_DL_LIBS})
endif()

if(ENABLE_OPENCV)
    target_link_libraries(modyn ${OpenCV_LIBS})
    target_link_libraries(modyn_core ${OpenCV_LIBS})
endif()

# API 支持
if(ENABLE_API)
    add_subdirectory(api)
    add_definitions(-DMODYN_ENABLE_API)
endif()

# 插件编译
if(ENABLE_PLUGINS AND BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

# 工具
add_subdirectory(tools)

# 示例
add_subdirectory(examples)

# 测试
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 安装配置
install(TARGETS modyn modyn_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# 创建插件目录
if(ENABLE_PLUGINS)
    install(DIRECTORY DESTINATION plugins)
    install(DIRECTORY DESTINATION plugins/onnx_runtime)
endif()

# 设置版本变量
set(MODYN_VERSION ${PROJECT_VERSION})
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" MODYN_VERSION_MATCH ${MODYN_VERSION})
set(MODYN_MAJOR_VERSION ${CMAKE_MATCH_1})
set(MODYN_MINOR_VERSION ${CMAKE_MATCH_2})
set(MODYN_PATCH_VERSION ${CMAKE_MATCH_3})

# 配置文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ModynConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ModynConfig.cmake"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ModynConfigVersion.cmake.in" 
    "${CMAKE_CURRENT_BINARY_DIR}/ModynConfigVersion.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ModynConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ModynConfigVersion.cmake"
    DESTINATION lib/cmake/Modyn
)

# pkg-config 文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modyn.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/modyn.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/modyn.pc"
    DESTINATION lib/pkgconfig
)

# 打包配置
set(CPACK_PACKAGE_NAME "modyn")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform Model Inference Service System")
set(CPACK_PACKAGE_VENDOR "Modyn Team")
set(CPACK_PACKAGE_CONTACT "team@modyn.ai")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "TGZ;DEB;RPM")

# DEB 包配置
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libgcc-s1 (>= 3.0), libpthread-stubs0-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM 包配置
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc >= 2.17")

# 组件
set(CPACK_COMPONENTS_ALL
    Unspecified
    runtime
    development
    plugins
    documentation
)

set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime Libraries")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Runtime libraries for Modyn")

set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "Headers and development files")

set(CPACK_COMPONENT_PLUGINS_DISPLAY_NAME "Plugins")
set(CPACK_COMPONENT_PLUGINS_DESCRIPTION "Inference engine plugins")

set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "API documentation and examples")

include(CPack)

# 状态信息
message(STATUS "Modyn Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  RKNN Backend: ${ENABLE_RKNN}")
message(STATUS "  OpenVINO Backend: ${ENABLE_OPENVINO}")
message(STATUS "  TensorRT Backend: ${ENABLE_TENSORRT}")
message(STATUS "  API Support: ${ENABLE_API}")
message(STATUS "  OpenCV Support: ${ENABLE_OPENCV}")
message(STATUS "  Plugin System: ${ENABLE_PLUGINS}")
message(STATUS "  Build Plugins: ${BUILD_PLUGINS}")
message(STATUS "  Tests: ${ENABLE_TESTS}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")

if(ENABLE_PLUGINS)
    message(STATUS "  Plugin Directory: ${CMAKE_INSTALL_PREFIX}/plugins")
endif() 