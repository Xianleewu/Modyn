#!/bin/bash

# Modyn 项目构建脚本
# 这个脚本用于快速构建整个项目

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    echo "Modyn 项目构建脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help          显示帮助信息"
    echo "  -c, --clean         清理构建目录"
    echo "  -d, --debug         调试模式构建"
    echo "  -r, --release       发布模式构建"
    echo "  -t, --test          运行测试"
    echo "  -j, --jobs <n>      并行构建任务数 (默认: $(nproc))"
    echo "  --enable-rknn       启用 RKNN 后端"
    echo "  --enable-openvino   启用 OpenVINO 后端"
    echo "  --enable-opencv     启用 OpenCV 支持"
    echo "  --enable-api        启用 API 支持"
    echo ""
    echo "示例:"
    echo "  $0                              # 默认构建"
    echo "  $0 -c -r --enable-rknn         # 清理后发布模式构建，启用 RKNN"
    echo "  $0 -d -t                        # 调试模式构建并运行测试"
    echo ""
}

# 默认设置
BUILD_TYPE="Debug"
CLEAN=false
RUN_TESTS=false
JOBS=$(nproc)
ENABLE_RKNN=false
ENABLE_OPENVINO=false
ENABLE_OPENCV=false
ENABLE_API=false

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --enable-rknn)
            ENABLE_RKNN=true
            shift
            ;;
        --enable-openvino)
            ENABLE_OPENVINO=true
            shift
            ;;
        --enable-opencv)
            ENABLE_OPENCV=true
            shift
            ;;
        --enable-api)
            ENABLE_API=true
            shift
            ;;
        *)
            print_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

print_info "开始构建 Modyn 项目..."
print_info "项目根目录: $PROJECT_ROOT"
print_info "构建目录: $BUILD_DIR"
print_info "构建类型: $BUILD_TYPE"
print_info "并行任务数: $JOBS"

# 清理构建目录
if [ "$CLEAN" = true ]; then
    print_info "清理构建目录..."
    rm -rf "$BUILD_DIR"
    print_success "构建目录已清理"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 构建 CMake 选项
CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$ENABLE_RKNN" = true ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_RKNN=ON"
    print_info "启用 RKNN 后端"
fi

if [ "$ENABLE_OPENVINO" = true ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_OPENVINO=ON"
    print_info "启用 OpenVINO 后端"
fi

if [ "$ENABLE_OPENCV" = true ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_OPENCV=ON"
    print_info "启用 OpenCV 支持"
fi

if [ "$ENABLE_API" = true ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_API=ON"
    print_info "启用 API 支持"
fi

# 总是启用测试
CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_TESTS=ON"

# 运行 CMake 配置
print_info "配置项目..."
if cmake $CMAKE_OPTIONS ..; then
    print_success "CMake 配置成功"
else
    print_error "CMake 配置失败"
    exit 1
fi

# 编译项目
print_info "编译项目..."
if make -j$JOBS; then
    print_success "编译成功"
else
    print_error "编译失败"
    exit 1
fi

# 运行测试
if [ "$RUN_TESTS" = true ]; then
    print_info "运行测试..."
    if ctest --output-on-failure; then
        print_success "所有测试通过"
    else
        print_error "测试失败"
        exit 1
    fi
fi

# 显示构建结果
print_info "构建结果:"
echo "  可执行文件目录: $BUILD_DIR/bin"
echo "  库文件目录: $BUILD_DIR/lib"
echo ""
echo "可用的示例程序:"
if [ -f "$BUILD_DIR/bin/examples/basic_inference" ]; then
    echo "  $BUILD_DIR/bin/examples/basic_inference"
fi
if [ -f "$BUILD_DIR/bin/examples/model_management" ]; then
    echo "  $BUILD_DIR/bin/examples/model_management"
fi
echo ""
echo "可用的工具:"
if [ -f "$BUILD_DIR/bin/tools/model_converter" ]; then
    echo "  $BUILD_DIR/bin/tools/model_converter"
fi
if [ -f "$BUILD_DIR/bin/tools/benchmark_tool" ]; then
    echo "  $BUILD_DIR/bin/tools/benchmark_tool"
fi

print_success "构建完成！"

# 提示如何运行示例
echo ""
print_info "运行示例:"
echo "  cd $BUILD_DIR"
echo "  ./bin/examples/basic_inference dummy_model.rknn"
echo "  ./bin/examples/model_management"
echo ""
print_info "运行工具:"
echo "  ./bin/tools/model_converter --help"
echo "" 