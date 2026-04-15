# 多阶段构建 - 支持 ARM64 和 AMD64
FROM --platform=$BUILDPLATFORM ubuntu:22.04 AS builder

ARG TARGETPLATFORM
ARG BUILDPLATFORM

ENV DEBIAN_FRONTEND=noninteractive

# 使用阿里云镜像源加速
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# 安装构建依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    qt6-base-dev \
    libqt6sql6-sqlite \
    libqt6sql6-mysql \
    libqt6sql6-psql \
    libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 复制源代码
COPY CMakeLists.txt .
COPY src/ src/

# 构建
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel

# 运行时镜像
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 使用阿里云镜像源
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# 安装运行时依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6core6 \
    libqt6network6 \
    libqt6sql6 \
    libqt6sql6-sqlite \
    libqt6sql6-mysql \
    libqt6sql6-psql \
    libqt6widgets6 \
    libqt6gui6 \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 从构建阶段复制可执行文件
COPY --from=builder /app/build/ChatServer .

# 环境变量
ENV DB_TYPE=sqlite
ENV DB_NAME=/app/data/chatserver.db
ENV HEADLESS=1

# 创建数据目录
RUN mkdir -p /app/data

# 启动脚本
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh

EXPOSE 9999

ENTRYPOINT ["/docker-entrypoint.sh"]
