#!/bin/bash

# ChatServer 启动和验证脚本
# 兼容 macOS 和 Windows (Git Bash/WSL)

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 检测操作系统
detect_os() {
    case "$(uname -s)" in
        Darwin*) OS="mac" ;;
        Linux*)  OS="linux" ;;
        MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
        *) OS="unknown" ;;
    esac
    print_info "检测到操作系统: $OS"
}

# 检查命令是否存在
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 安装 Docker
install_docker() {
    print_info "正在安装 Docker..."
    
    case "$OS" in
        mac)
            if command_exists brew; then
                brew install --cask docker
                print_warn "请手动启动 Docker Desktop 应用"
                print_warn "启动后按回车继续..."
                read -r
            else
                print_error "请先安装 Homebrew: /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                print_error "或者手动下载 Docker Desktop: https://www.docker.com/products/docker-desktop"
                exit 1
            fi
            ;;
        windows)
            print_error "请手动安装 Docker Desktop: https://www.docker.com/products/docker-desktop"
            print_error "安装完成后重新运行此脚本"
            exit 1
            ;;
        linux)
            if command_exists apt-get; then
                sudo apt-get update
                sudo apt-get install -y docker.io docker-compose
                sudo systemctl start docker
                sudo usermod -aG docker "$USER"
                print_warn "已添加当前用户到 docker 组，可能需要重新登录"
            elif command_exists yum; then
                sudo yum install -y docker docker-compose
                sudo systemctl start docker
            else
                print_error "请手动安装 Docker: https://docs.docker.com/engine/install/"
                exit 1
            fi
            ;;
    esac
}

# 检查 Docker 是否运行
check_docker_running() {
    if ! docker info >/dev/null 2>&1; then
        print_warn "Docker 未运行"
        
        case "$OS" in
            mac)
                print_info "尝试启动 Docker Desktop..."
                open -a Docker 2>/dev/null || true
                print_info "等待 Docker 启动 (最多60秒)..."
                for i in {1..60}; do
                    if docker info >/dev/null 2>&1; then
                        print_info "Docker 已启动"
                        return 0
                    fi
                    sleep 1
                done
                print_error "Docker 启动超时，请手动启动 Docker Desktop"
                exit 1
                ;;
            *)
                print_error "请启动 Docker 服务"
                exit 1
                ;;
        esac
    fi
}


# 检查 docker-compose
check_docker_compose() {
    if command_exists docker-compose; then
        COMPOSE_CMD="docker-compose"
    elif docker compose version >/dev/null 2>&1; then
        COMPOSE_CMD="docker compose"
    else
        print_error "docker-compose 未找到"
        case "$OS" in
            mac)
                print_info "Docker Desktop 应该自带 docker-compose，请确保 Docker Desktop 已正确安装"
                ;;
            *)
                print_info "尝试安装 docker-compose..."
                sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
                sudo chmod +x /usr/local/bin/docker-compose
                COMPOSE_CMD="docker-compose"
                ;;
        esac
    fi
    print_info "使用 compose 命令: $COMPOSE_CMD"
}

# 构建并启动服务
start_server() {
    print_info "构建并启动 ChatServer..."
    $COMPOSE_CMD up -d --build
    
    print_info "等待服务启动..."
    sleep 5
}

# 验证服务
verify_server() {
    print_info "验证服务状态..."
    
    # 检查容器状态
    if $COMPOSE_CMD ps | grep -q "chat-server.*Up\|chat-server.*running"; then
        print_info "容器运行正常"
    else
        print_error "容器未正常运行"
        $COMPOSE_CMD logs chatserver
        exit 1
    fi
    
    # 测试端口连接
    print_info "测试端口 9999 连接..."
    
    local connected=false
    for i in {1..10}; do
        if (echo > /dev/tcp/localhost/9999) 2>/dev/null; then
            connected=true
            break
        elif command_exists nc; then
            if nc -z localhost 9999 2>/dev/null; then
                connected=true
                break
            fi
        fi
        sleep 1
    done
    
    if [ "$connected" = true ]; then
        print_info "端口 9999 连接成功"
    else
        print_warn "端口连接测试失败，查看日志..."
        $COMPOSE_CMD logs --tail=20 chatserver
    fi
}

# 显示状态
show_status() {
    echo ""
    echo "=========================================="
    print_info "ChatServer 状态"
    echo "=========================================="
    $COMPOSE_CMD ps
    echo ""
    print_info "最近日志:"
    $COMPOSE_CMD logs --tail=10 chatserver
    echo ""
    echo "=========================================="
    print_info "服务地址: localhost:9999"
    print_info "停止服务: $COMPOSE_CMD down"
    print_info "查看日志: $COMPOSE_CMD logs -f chatserver"
    echo "=========================================="
}

# 主函数
main() {
    echo "=========================================="
    echo "  ChatServer 启动脚本"
    echo "=========================================="
    
    detect_os
    
    # 检查并安装 Docker
    if ! command_exists docker; then
        print_warn "Docker 未安装"
        install_docker
    else
        print_info "Docker 已安装"
    fi
    
    check_docker_running
    check_docker_compose
    start_server
    verify_server
    show_status
}

# 运行
main "$@"
