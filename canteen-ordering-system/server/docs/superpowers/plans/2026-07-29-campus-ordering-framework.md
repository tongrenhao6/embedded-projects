# 校园点餐平台框架实现计划

> **面向 AI 代理的工作者：** 使用内联执行方式逐任务完成本计划；步骤使用复选框语法跟踪进度。

**目标：** 创建可测试的 C 语言点餐服务端核心和可扩展的 LVGL 客户端目录框架。

**架构：** 服务端将菜单文件加载为数组，并用链表保存单个连接的购物车；TCP 层调用这些核心模块。客户端以应用状态、网络客户端和独立 UI 组件分层，图片资源与 C 源码分离。

**技术栈：** C11、POSIX Socket、文件 IO、LVGL 8.2、GCC/Make。

---

### 任务 1：初始化工程边界

**文件：**
- 创建：`README.md`
- 创建：`common/include/order_protocol.h`
- 创建：`server/Makefile`

- [ ] 定义共享协议常量、端口和行长度。
- [ ] 提供仅构建服务端和运行测试的 Makefile。

### 任务 2：菜单和购物车核心

**文件：**
- 创建：`server/include/menu.h`
- 创建：`server/include/cart.h`
- 创建：`server/src/menu.c`
- 创建：`server/src/cart.c`
- 创建：`server/tests/test_menu.c`
- 创建：`server/tests/test_cart.c`

- [ ] 先编写菜单加载与链表累计总价的失败测试。
- [ ] 运行 `make test` 确认测试因函数缺失而失败。
- [ ] 实现最小菜单加载和购物车操作，重跑 `make test`。

### 任务 3：TCP 服务端和示例菜单

**文件：**
- 创建：`server/src/server_main.c`
- 创建：`server/data/menu.txt`
- 创建：`server/data/orders.txt`

- [ ] 实现 `MENU`、`ADD`、`REMOVE`、`CART`、`SUBMIT` 命令处理。
- [ ] 将订单以追加方式写入订单文件。
- [ ] 运行 `make server`，确认服务端可编译。

### 任务 4：LVGL 客户端接入预留

**文件：**
- 创建：`client/README.md`
- 创建：`client/assets/images/README.md`

- [ ] 说明未来客户端状态、网络接口和三栏 UI 的模块边界。
- [ ] 不创建或修改现有 `lv_port_linux-v8.2` 工程中的文件。

### 任务 5：验证和使用说明

**文件：**
- 修改：`README.md`

- [ ] 运行 `make test` 和 `make server`。
- [ ] 写明阿里云部署、开发板集成和图片放置路径。
