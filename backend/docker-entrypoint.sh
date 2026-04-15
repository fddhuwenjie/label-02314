#!/bin/bash

# 启用无头模式
export HEADLESS=1

# 启动应用
exec ./ChatServer "$@"
