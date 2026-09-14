# 校园点餐平台

> 项目文件用途和修改位置请先阅读：[docs/PROJECT_STRUCTURE.md](docs/PROJECT_STRUCTURE.md)

这是一个面向系统编程阶段项目的最小校园点餐平台。服务端部署在阿里云 Linux 服务器，6818 开发板上的 LVGL 程序作为 TCP 客户端。

## 当前范围

- `server/data/menu.txt`：文件 IO 读取菜品。
- `server/src/cart.c`：链表购物车，负责增删、总价和内存释放。
- `server/src/order_service.c`：处理点餐命令并在下单时写入订单文件。
- `server/src/server_main.c`：TCP 服务端，监听 `18080` 端口。
- `common/include/order_protocol.h`：端侧和服务端共享的协议常量。

不包含数据库、用户登录、真实支付和管理后台。

## 工程结构

```text
服务器/
  common/include/       共享协议
  server/               独立 C 语言服务端和单元测试
  client/               仅预留端侧接入位置，不包含 LVGL 源码
  docs/                 设计与实现计划
```

开发板客户端位于同级目录 `../lv_port_linux-v8.2`。实际的 LVGL 点餐界面和 TCP 客户端代码在该工程的 `ui/ui_order.c`、`ui/order_client.c` 中。

## 阿里云部署

在服务器进入 `server` 目录后执行：

```bash
make server
./build/order_server data/menu.txt data/orders.txt
```

阿里云安全组和 Linux 防火墙需要放行 TCP `18080`。实际部署时应将 `orders.txt` 置于应用可写目录。

## 协议示例

每条消息以换行结束：

```text
MENU\n
ADD|1\n
CART\n
REMOVE|1\n
SUBMIT\n
```

服务端会返回 `OK|...` 或 `ERROR|...`。价格的单位是元，例如 `12.00` 表示 12.00 元；LVGL 的 + 和 - 按钮每点击一次，分别发送一次 `ADD|编号` 或 `REMOVE|编号`。

## 验证

在 Linux 上执行：

```bash
cd server
make test
make server
make test-multi
```

`make test-multi` 会启动 epoll 服务端，并验证两个同时保持连接的客户端拥有独立购物车，且下单后使用连续订单号。
