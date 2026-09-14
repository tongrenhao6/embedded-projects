# 云嵌食堂项目结构说明

## 1. 项目组成

本项目由两个程序组成：

```text
6818 开发板上的 LVGL 客户端
            |
            | TCP: 118.31.185.167:18080
            v
阿里云 Linux 服务器上的 C 服务端
```

服务器负责菜单读取、购物车保存、价格计算和订单文件保存；开发板负责界面显示和触摸操作。

LVGL 客户端工程与本项目同级，位于：

```text
F:\gyb\GZFX2603-葛玉彬-校园点餐\3-项目源码\lv_port_linux-v8.2
```

## 2. 服务器项目目录

```text
服务器/
├─ common/
│  └─ include/order_protocol.h       客户端和服务端共用的协议常量
├─ server/
│  ├─ include/                       服务端头文件
│  │  ├─ menu.h                      菜单结构体和菜单函数声明
│  │  ├─ cart.h                      链表购物车结构体和函数声明
│  │  └─ order_service.h             点餐业务函数声明
│  ├─ src/                           服务端 C 源文件
│  │  ├─ server_main.c               TCP/epoll 服务器入口，多客户端连接
│  │  ├─ menu.c                      从 menu.txt 读取和查找菜品
│  │  ├─ cart.c                      添加、删除、统计总价、释放链表
│  │  └─ order_service.c              解析 MENU/ADD/REMOVE/CART/SUBMIT 命令
│  ├─ data/                          运行时数据，不是源代码
│  │  ├─ menu.txt                    菜单数据，格式：id|分类|名称|价格
│  │  └─ orders.txt                  已提交订单记录
│  ├─ tests/                          服务端单元测试和多客户端测试
│  │  └─ fixtures/                   测试使用的菜单、订单样例
│  ├─ build/                          编译生成的目标文件和可执行文件
│  └─ Makefile                        服务端编译、测试、清理命令
├─ client/                            早期终端客户端，主要用于学习和测试协议
│  ├─ src/client_main.c              命令行点餐客户端
│  ├─ include/client_main.h          命令行客户端头文件
│  └─ README.md                      命令行客户端说明
├─ docs/
│  ├─ PROJECT_STRUCTURE.md           本文件
│  ├─ superpowers/specs/             项目设计说明
│  └─ superpowers/plans/             项目实现计划
└─ README.md                          项目总体说明
```

## 3. 平时主要修改哪些文件

| 需求 | 修改文件 |
|---|---|
| 增加、删除或修改菜品 | `server/data/menu.txt` |
| 修改菜单读取 | `server/src/menu.c`、`server/include/menu.h` |
| 修改购物车规则 | `server/src/cart.c`、`server/include/cart.h` |
| 修改 TCP 命令处理 | `server/src/order_service.c`、`server/include/order_service.h` |
| 修改客户端连接地址或收发逻辑 | LVGL 工程的 `ui/order_client.c`、`ui/order_client.h` |
| 修改点餐界面 | LVGL 工程的 `ui/ui_order.c` |

## 4. 协议速查

客户端每次发送一行命令，以换行符结束：

```text
MENU
ADD|1
REMOVE|1
CART
SUBMIT
```

价格单位是元。服务端返回菜单、购物车总价或订单号，LVGL 客户端根据返回结果更新界面。

## 5. 哪些文件不要手动修改

- `server/build/`：服务端编译生成目录。
- LVGL 工程的 `build/`、`build_host/`：LVGL 编译生成目录。
- `lvgl/`：LVGL 库源码，本项目不修改。
- `ui/images/ui_img_*.c`：图片转换生成的 C 数组；换图片时应替换资源并重新转换。

## 6. 服务端常用命令

```bash
cd server
make server
./build/order_server data/menu.txt data/orders.txt
```

后台运行：

```bash
nohup ./build/order_server data/menu.txt data/orders.txt > server.log 2>&1 &
```
