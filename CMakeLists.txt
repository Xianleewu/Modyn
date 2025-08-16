cmake_minimum_required(VERSION 3.14)
project(modyn C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MODYN_BUILD_EXAMPLES "Build example programs" ON)
option(MODYN_BUILD_TESTS "Build tests" ON)
option(MODYN_BUILD_PLUGINS "Build sample driver plugins" ON)
option(MODYN_BUILD_SHARED "Build shared library (libmodyn.so)" ON)
option(MODYN_BUILD_STATIC "Build static library (libmodyn.a)" OFF)

# 核心库源文件列表
set(MODYN_CORE_SOURCES
  src/pipeline/modyn_pipeline.c      # Pipeline核心实现
  src/pipeline/dummy_node.c          # Dummy节点示例
  src/component_manager.c            # 组件管理器
  src/modyn_device_mgmt.c            # 设备管理
  src/modyn_model_loading.c          # 模型加载
  src/modyn_stubs.c                  # 通用 stub（convert、clone、zero-copy）
  src/modyn_autoload.c               # 启动自动加载插件
  src/devices/dummy/dummy_device.c   # 底层推理设备（示例）
)

# 创建核心库目标
if(MODYN_BUILD_SHARED)
  add_library(modyn SHARED ${MODYN_CORE_SOURCES})
  set_target_properties(modyn PROPERTIES
    OUTPUT_NAME "modyn"
    VERSION 1.0.0
    SOVERSION 1
    POSITION_INDEPENDENT_CODE ON
  )
  target_compile_options(modyn PRIVATE -fPIC -Wall -Wextra)
  target_include_directories(modyn PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(modyn PRIVATE dl)
  message(STATUS "Building shared library: libmodyn.so")
endif()

if(MODYN_BUILD_STATIC)
  add_library(modyn_static STATIC ${MODYN_CORE_SOURCES})
  set_target_properties(modyn_static PROPERTIES
    OUTPUT_NAME "modyn"
    POSITION_INDEPENDENT_CODE ON
  )
  target_compile_options(modyn_static PRIVATE -fPIC -Wall -Wextra)
  target_include_directories(modyn_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  message(STATUS "Building static library: libmodyn.a")
endif()

# 为了向后兼容，创建一个别名
if(MODYN_BUILD_SHARED AND MODYN_BUILD_STATIC)
  add_library(modyn_dummy ALIAS modyn)
  add_library(modyn_dummy_static ALIAS modyn_static)
  message(STATUS "Building both shared and static libraries")
elseif(MODYN_BUILD_SHARED)
  add_library(modyn_dummy ALIAS modyn)
  message(STATUS "Building shared library only")
elseif(MODYN_BUILD_STATIC)
  add_library(modyn_dummy ALIAS modyn_static)
  message(STATUS "Building static library only")
else()
  message(FATAL_ERROR "At least one library type must be selected (MODYN_BUILD_SHARED or MODYN_BUILD_STATIC)")
endif()

if(MODYN_BUILD_EXAMPLES)
  add_executable(dummy_infer examples/dummy_infer_main.c)
  target_include_directories(dummy_infer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  
  if(MODYN_BUILD_SHARED AND MODYN_BUILD_STATIC)
    # 如果同时构建两种库，优先使用共享库
    target_link_libraries(dummy_infer PRIVATE modyn)
    target_link_options(dummy_infer PRIVATE -Wl,-export-dynamic)
  elseif(MODYN_BUILD_SHARED)
    target_link_libraries(dummy_infer PRIVATE modyn)
    target_link_options(dummy_infer PRIVATE -Wl,-export-dynamic)
  elseif(MODYN_BUILD_STATIC)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_link_libraries(dummy_infer PRIVATE -Wl,--whole-archive modyn_static -Wl,--no-whole-archive)
      target_link_options(dummy_infer PRIVATE -Wl,-export-dynamic)
    else()
      target_link_libraries(dummy_infer PRIVATE modyn_static)
    endif()
  endif()
endif()

if(MODYN_BUILD_TESTS)

  

  
  # 综合测试程序
  add_executable(test_all_in_one tests/test_all_in_one.c)
  target_include_directories(test_all_in_one PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  
  if(MODYN_BUILD_SHARED AND MODYN_BUILD_STATIC)
    # 如果同时构建两种库，优先使用共享库
    target_link_libraries(test_all_in_one PRIVATE modyn)
    target_link_options(test_all_in_one PRIVATE -Wl,-export-dynamic)
  elseif(MODYN_BUILD_SHARED)
    target_link_libraries(test_all_in_one PRIVATE modyn)
    target_link_options(test_all_in_one PRIVATE -Wl,-export-dynamic)
  elseif(MODYN_BUILD_STATIC)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_link_libraries(test_all_in_one PRIVATE -Wl,--whole-archive modyn_static -Wl,--no-whole-archive)
      target_link_options(test_all_in_one PRIVATE -Wl,-export-dynamic)
    else()
      target_link_libraries(test_all_in_one PRIVATE modyn_static)
    endif()
  endif()
endif()

if(MODYN_BUILD_PLUGINS)
  # 创建plugins目录
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugins)
  
  # 动态库统一放在plugins目录里
  add_library(modyn_dummy_gpu SHARED
    src/devices/dummy/dummy_device_gpu.c
  )
  target_include_directories(modyn_dummy_gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(modyn_dummy_gpu PRIVATE MODYN_ENABLE_LOGGING)
  target_link_libraries(modyn_dummy_gpu PRIVATE dl)
  
  # 设置输出目录为plugins
  set_target_properties(modyn_dummy_gpu PROPERTIES 
    OUTPUT_NAME "modyn_dummy_gpu"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugins
  )
  
  # 创建plugins目录的符号链接到源码目录，便于测试时查找
  add_custom_command(TARGET modyn_dummy_gpu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${CMAKE_CURRENT_BINARY_DIR}/plugins 
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMENT "Creating plugins directory symlink"
  )
endif()

# ======================== 安装规则 ========================
if(MODYN_BUILD_SHARED)
  install(TARGETS modyn
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
  )
  install(FILES src/modyn.h DESTINATION include/modyn)
  install(FILES src/component_manager.h DESTINATION include/modyn)
  install(FILES src/pipeline/modyn_pipeline.h DESTINATION include/modyn/pipeline)
  install(FILES src/pipeline/dummy_node.h DESTINATION include/modyn/pipeline)
endif()

if(MODYN_BUILD_STATIC)
  install(TARGETS modyn_static
    ARCHIVE DESTINATION lib
  )
  install(FILES src/component_manager.h DESTINATION include/modyn)
  install(FILES src/pipeline/modyn_pipeline.h DESTINATION include/modyn/pipeline)
  install(FILES src/pipeline/dummy_node.h DESTINATION include/modyn/pipeline)
endif()

# ======================== 配置信息 ========================
message(STATUS "Modyn Framework Configuration:")
message(STATUS "  Build Examples: ${MODYN_BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${MODYN_BUILD_TESTS}")
message(STATUS "  Build Plugins: ${MODYN_BUILD_PLUGINS}")
message(STATUS "  Build Shared Library: ${MODYN_BUILD_SHARED}")
message(STATUS "  Build Static Library: ${MODYN_BUILD_STATIC}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C Standard: ${CMAKE_C_STANDARD}")

# Pipeline示例程序
if(MODYN_BUILD_EXAMPLES)
  add_executable(pipeline_example src/pipeline_example.c)
  target_include_directories(pipeline_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  # 使用--whole-archive确保静态库中的所有符号都被链接进去
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(pipeline_example PRIVATE -Wl,--whole-archive modyn_dummy -Wl,--no-whole-archive)
    target_link_options(pipeline_example PRIVATE -Wl,-export-dynamic)
  else()
    target_link_libraries(pipeline_example PRIVATE modyn_dummy)
  endif()
endif()


