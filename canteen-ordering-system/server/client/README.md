# 开发板端预留目录

本目录不包含 LVGL 实现，仅保留早期命令行客户端代码，用于学习 TCP 收发和调试协议。实际 LVGL 客户端在同级目录 `../lv_port_linux-v8.2`。

！！！！！只用于客户端尝试以及实验，非LVGL客户端真正实验源码！！！！！！

后续接入时建议从现有 LVGL 工程中新建独立模块，而不是把所有逻辑写在 `main.c`：

- `app_state.c`：菜单、购物车和页面状态。
- `network_client.c`：TCP 连接、请求发送和响应解析。
- `ui/menu_panel.c`：中间菜品卡片区。
- `ui/cart_panel.c`：右侧购物车区。
- `ui/order_screen.c`：800x480 三栏页面组装。

这些模块以后再创建，避免当前提前触碰 LVGL 工程。
