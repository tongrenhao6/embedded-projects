#ifndef ORDER_CLIENT_H
#define ORDER_CLIENT_H

#include <stddef.h>

#define ORDER_CLIENT_MENU_MAX 16

/*
 * 服务器返回的一道菜单数据。
 * 结构体中不保存菜品图片，因为图片保存在开发板本地，由 ui_order.c 根据 id 绑定。
 */
typedef struct {
    int id;             /* 菜品唯一编号。 */
    char category[32];  /* 菜品分类，例如“主食”。 */
    char name[64];      /* 菜品名称，例如“鸡腿饭”。 */
    float price;        /* 菜品单价，单位为元。 */
} order_menu_item_t;

/* TCP 客户端连接状态。socket_fd 为 -1 表示当前未连接服务器。 */
typedef struct {
    int socket_fd;
} order_client_t;

/*
 * 功能：创建 TCP 套接字并连接服务器。
 * 参数：client 为客户端结构体；server_ip 为服务器 IPv4 地址；server_port 为服务器端口。
 * 返回值：成功返回 0，失败返回 -1。
 */
int order_client_connect(order_client_t *client, const char *server_ip, unsigned short server_port);

/*
 * 功能：关闭当前 TCP 连接，并把 client->socket_fd 设为 -1。
 * 参数：client 为要关闭的客户端结构体，允许传入 NULL。
 * 返回值：无。
 */
void order_client_close(order_client_t *client);

/*
 * 功能：发送 MENU 命令，从服务器加载菜单。
 * 参数：client 为已连接客户端；menu 为保存菜单的数组；menu_size 为数组容量；
 *        menu_count 为输出参数，保存实际收到的菜品数量。
 * 返回值：成功返回 0，失败返回 -1。
 */
int order_client_load_menu(order_client_t *client, order_menu_item_t menu[], size_t menu_size, size_t *menu_count);

/*
 * 功能：发送 ADD|菜品ID 命令，增加一份菜品。
 * 参数：client 为已连接客户端；dish_id 为菜品 ID；total 为输出参数，保存最新总价。
 * 返回值：成功返回 0，失败返回 -1。
 */
int order_client_add(order_client_t *client, int dish_id, float *total);

/*
 * 功能：发送 REMOVE|菜品ID 命令，删除一份菜品。
 * 参数：client 为已连接客户端；dish_id 为菜品 ID；total 为输出参数，保存最新总价。
 * 返回值：成功返回 0，失败返回 -1。
 */
int order_client_remove(order_client_t *client, int dish_id, float *total);

/*
 * 功能：发送 SUBMIT 命令，提交购物车订单。
 * 参数：client 为已连接客户端；order_id 和 total 为输出参数，分别保存订单号和总价。
 * 返回值：成功返回 0，失败返回 -1。
 */
int order_client_submit(order_client_t *client, unsigned int *order_id, float *total);

#endif
