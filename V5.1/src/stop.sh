#!/bin/bash

echo "========================================="
echo "  Stopping NetDisk Server"
echo "========================================="

# 查找netdisk_server进程
PID=$(pgrep netdisk_server)

if [ -z "$PID" ]; then
    echo "❌ NetDisk server is not running"
    exit 1
fi

echo "📡 Found server process: PID=$PID"
echo "🛑 Sending SIGTERM signal..."

# 发送终止信号
kill -SIGTERM $PID

# 等待进程退出
for i in {1..10}; do
    if ! ps -p $PID > /dev/null 2>&1; then
        echo "✅ Server stopped successfully"
        exit 0
    fi
    sleep 1
done

# 如果10秒后仍未退出，强制终止
echo "⚠️  Server did not stop gracefully, forcing..."
kill -9 $PID 2>/dev/null
sleep 1

if ! ps -p $PID > /dev/null 2>&1; then
    echo "✅ Server force stopped"
else
    echo "❌ Failed to stop server"
    exit 1
fi
