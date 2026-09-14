#!/usr/bin/env bash
set -eu

cd "$(dirname "$0")/.."

./build/order_server data/menu.txt build/orders_multi_client_linux_test.txt >/tmp/cloud_embedded_canteen_server.log 2>&1 &
server_pid=$!

cleanup() {
    kill "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT

sleep 0.3

python3 - <<'PY'
import socket


def command(client, text):
    client.sendall((text + "\n").encode())
    return client.recv(1024).decode().strip()


first = socket.create_connection(("127.0.0.1", 18080))
assert command(first, "ADD|1") == "OK|CART|12.00"

# 第一个客户端不断开，第二个客户端仍然必须马上被 epoll 服务器处理。
second = socket.create_connection(("127.0.0.1", 18080))
assert command(second, "CART") == "OK|CART|0.00"
assert command(second, "ADD|2") == "OK|CART|15.00"
assert command(second, "SUBMIT") == "OK|ORDER|1|15.00"
assert command(first, "SUBMIT") == "OK|ORDER|2|12.00"

first.close()
second.close()
print("epoll multi-client integration test passed")
PY
