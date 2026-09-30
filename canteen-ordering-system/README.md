# 云嵌食堂 · 校园智能点餐系统

嵌入式触摸屏客户端 + 云端 TCP 服务端的校园点餐平台。客户端在 6818 Linux 开发板上用 LVGL 8.2 跑点餐界面，通过 TCP 连接部署在阿里云上的 C 语言 epoll 服务端，完成菜单浏览、购物车、结算与订单落盘全流程。

## 系统架构

```
6818 开发板 (800×480 触摸屏)
        │
        │ TCP 18080  MENU / ADD / REMOVE / CART / SUBMIT
        ▼
阿里云 Linux 服务器 (epoll, 最多 16 并发客户端)
        │
        ├── menu.txt     菜单文件 (12 道菜)
        ├── 单链表购物车  每个连接独立维护
        └── orders.txt   已提交订单追加落盘
```

## 已实现功能

- **LVGL 8.2 点餐界面**：首页 / 点餐页 / 购物车 / 下单成功弹窗，分类切换、菜品卡片三列滚动、加减按钮、动态合计
- **epoll TCP 服务端**：单进程单线程，事件循环支持 16 路并发客户端
- **每连接独立购物车**：用单链表保存，连接断开即释放
- **文本应用层协议**：`MENU / ADD|ID / REMOVE|ID / CART / SUBMIT`，`\n` 分隔
- **订单落盘**：先追加写入 `orders.txt`，成功后再清空购物车，避免写入失败丢单
- **客户端**：TCP 断线提示、空购物车提示、下单成功弹窗

## 目录结构

```
canteen-ordering-system/
├── client/                  # 6818 开发板触摸屏客户端
│   ├── main.c               # LVGL + framebuffer/evdev 入口
│   ├── lv_conf.h            # LVGL 配置
│   ├── Makefile
│   ├── ui/                  # 业务 UI（核心）
│   │   ├── ui_order.c       # 点餐/购物车界面主逻辑 (~44KB)
│   │   ├── order_client.c   # TCP 客户端封装
│   │   ├── ui_helpers.c
│   │   ├── screens/  components/  fonts/  images/
│   └── lv_drivers/          # LVGL 显示/输入驱动适配
├── server/                  # 云端 C 语言服务端
│   ├── server/
│   │   ├── src/server_main.c      # epoll 主循环
│   │   ├── src/order_service.c    # 业务逻辑
│   │   ├── src/cart.c             # 单链表购物车
│   │   ├── src/menu.c             # 菜单加载
│   │   ├── Makefile
│   │   └── tests/                 # 单元测试 + 多客户端压力测试脚本
│   └── common/include/order_protocol.h
└── docs/
    └── 云嵌食堂项目说明文档.md
```

> 注：`lvgl/` 官方源码与 `lv_lib_100ask/` 第三方库未上传，按 `lv_port_linux-v8.2` 原仓库的 submodule 方式拉取即可。

## 编译与运行

### 服务端（Linux / 阿里云）

```bash
cd server/server
make
./order_server          # 监听 0.0.0.0:18080
# 多客户端压力测试
bash tests/test_multi_client.sh
```

### 客户端（6818 开发板）

需要先准备 LVGL 8.2 与 lv_port_linux 环境，然后：

```bash
cd client
make
./demo_800              # 连接服务器 IP:18080
```

## 关键技术点

- **epoll ET/LT 模式** + 每连接会话状态机
- **单链表购物车**：O(1) 插入删除，按菜品 ID 去重计数
- **LVGL 事件回调**：触摸 → 协议命令 → 服务端响应 → UI 刷新
- **订单可靠性**：先落盘再清购物车，避免崩溃丢单
- **16 路并发**：单线程事件循环，无锁设计

## 技术栈

C / Linux / LVGL 8.2 / framebuffer / evdev / TCP Socket / epoll / 阿里云 ECS
