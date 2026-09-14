/* 开发板端 TCP 客户端：负责与阿里云点餐服务器建立连接、收发命令和解析响应。 */

#include "order_client.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define ORDER_CLIENT_RESPONSE_SIZE 1024

/**
 * 函数功能：把一条完整的文本命令发送给服务器。
 *
 * send() 一次调用不一定能发送完全部数据，因此使用循环，直到整条命令全部发送完成。
 *
 * 参数：
 *   client  - 已连接服务器的客户端结构体，内部保存 socket 文件描述符。
 *   command - 要发送的字符串命令，例如 "ADD|10\n"。
 *
 * 返回值：
 *   0  - 命令全部发送成功。
 *   -1 - 发送失败或连接已断开。
 */
static int send_command(order_client_t *client, const char *command)
{
    size_t length = strlen(command);
    size_t sent = 0;

    while(sent < length) {
        // 发送命令片段
        ssize_t result = send(client->socket_fd, command + sent, length - sent, 0);

        if(result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }

    return 0;
}

/**
 * 函数功能：接收服务器响应，并保存为 C 字符串。
 *
 * MENU 响应包含多行 ITEM 数据，直到收到 "OK|MENU\n" 才表示菜单结束；
 * ADD、REMOVE、SUBMIT 等命令的响应只有一行，收到第一个换行符即可结束。
 *
 * 参数：
 *   client        - 已连接服务器的客户端结构体。
 *   response      - 用于保存服务器响应的字符数组。
 *   response_size - response 数组的总大小，函数会预留一个字节保存字符串结尾 '\0'。
 *   is_menu       - 1 表示正在接收 MENU 的多行响应；0 表示接收普通单行响应。
 *
 * 返回值：
 *   0  - 接收到了完整响应。
 *   -1 - 接收失败、服务器断开连接或响应长度超过数组容量。
 */
static int receive_response(order_client_t *client, char *response, size_t response_size, int is_menu)
{
    size_t used = 0;

    if(response_size == 0) {
        return -1;
    }

    while(used < response_size - 1) {
        // 接收服务器回传过来的命令
        ssize_t result = recv(client->socket_fd, response + used, response_size - used - 1, 0);

        if(result <= 0) {
            return -1;
        }

        used += (size_t)result;
        response[used] = '\0';
        // 判断是否接收到了完整响应
        if((is_menu && strstr(response, "OK|MENU\n") != NULL) ||
           (!is_menu && strchr(response, '\n') != NULL)) { //判断是否接收到了完整响应
            return 0;// --->115行
        }
    }

    return -1;
}

/**
 * 函数功能：完成一次“发送命令 -> 接收响应”的 TCP 请求。
 *
 * 这是 ADD、REMOVE、SUBMIT 和 MENU 等业务函数共用的内部函数。
 * 调用前会检查客户端是否已经成功连接服务器。
 *
 * 参数：
 *   client        - 客户端结构体，socket_fd 必须大于等于 0。
 *   command       - 要发送的完整协议命令，末尾必须带换行符。
 *   response      - 保存服务器响应的字符数组。
 *   response_size - response 数组的大小。
 *   is_menu       - 1 表示按多行 MENU 响应接收；0 表示按普通单行响应接收。
 *
 * 返回值：
 *   0  - 命令发送和响应接收都成功。
 *   -1 - 客户端未连接、发送失败或接收失败。
 */
static int request(order_client_t *client, const char *command, char *response, size_t response_size, int is_menu)
{
    if(client == NULL || client->socket_fd < 0) { //判断客户端是否已连接服务器
        return -1;
    }

    if(send_command(client, command) != 0) {  //判断有没有将完整命令发送给服务器
        return -1;
    }

    return receive_response(client, response, response_size, is_menu);// --->249行
}

/**
 * 函数功能：创建 TCP 套接字并连接阿里云服务器。
 *
 * 连接成功后，得到的 socket 文件描述符会保存到 client->socket_fd，
 * 后续 ADD、REMOVE、SUBMIT 等函数都会使用这个连接。
 *
 * 参数：
 *   client      - 客户端结构体地址，用于保存连接成功后的 socket_fd。
 *   server_ip   - 服务器 IPv4 地址字符串，例如 "118.31.185.167"。
 *   server_port - 服务器端口号，例如 18080。
 *
 * 返回值：
 *   0  - 成功连接服务器。
 *   -1 - 参数为空、创建套接字失败、IP 格式错误或连接服务器失败。
 */
int order_client_connect(order_client_t *client, const char *server_ip, unsigned short server_port)
{
    struct sockaddr_in server_address;
    int socket_fd;

    if(client == NULL || server_ip == NULL) {
        return -1;
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0) {
        return -1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    if(inet_pton(AF_INET, server_ip, &server_address.sin_addr) != 1 ||
       connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) {
        close(socket_fd);
        return -1;
    }

    client->socket_fd = socket_fd;
    return 0;
}

/**
 * 函数功能：关闭当前 TCP 连接，并把 socket_fd 恢复为 -1。
 *
 * 参数：
 *   client - 要关闭连接的客户端结构体；传入 NULL 时函数不执行任何操作。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
void order_client_close(order_client_t *client)
{
    if(client != NULL && client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
}

/**
 * 函数功能：向服务器发送 MENU 命令，接收菜单并解析为菜单结构体数组。
 *
 * 服务器返回的每一道菜格式为：ITEM|菜品ID|分类|名称|价格。
 * 本函数逐行解析 ITEM 数据，直到读取到 OK|MENU 为止。
 *
 * 参数：
 *   client     - 已连接服务器的客户端结构体。
 *   menu       - 用于保存菜单数据的数组。
 *   menu_size  - menu 数组最多能保存多少道菜。
 *   menu_count - 输出参数，成功时写入实际解析出的菜品数量。
 *
 * 返回值：
 *   0  - 菜单请求成功，menu 和 *menu_count 已写入有效数据。
 *   -1 - 参数无效、网络请求失败、响应格式错误、菜单超过数组容量或菜单为空。
 */
int order_client_load_menu(order_client_t *client, order_menu_item_t menu[], size_t menu_size, size_t *menu_count)
{
    char response[ORDER_CLIENT_RESPONSE_SIZE];
    char *line;
    size_t count = 0;

    if(menu == NULL || menu_count == NULL || menu_size == 0 ||
       request(client, "MENU\n", response, sizeof(response), 1) != 0) {
        return -1;
    }

    line = strtok(response, "\r\n");
    while(line != NULL) {
        if(strncmp(line, "ITEM|", 5) == 0) {
            order_menu_item_t item;

            if(count >= menu_size ||
               sscanf(line, "ITEM|%d|%31[^|]|%63[^|]|%f", &item.id, item.category,
                      item.name, &item.price) != 4) {
                return -1;
            }
            menu[count++] = item;
        } else if(strcmp(line, "OK|MENU") != 0) {
            return -1;
        }

        line = strtok(NULL, "\r\n");
    }

    if(count == 0) {
        return -1;
    }

    *menu_count = count;
    return 0;
}

/**
 * 函数功能：向服务器发送 ADD 命令，将指定菜品加入当前客户端的购物车。
 *
 * 参数：
 *   client  - 已连接服务器的客户端结构体。
 *   dish_id - 要添加的菜品 ID，例如泡芙的 ID 为 10。
 *   total   - 输出参数，成功时写入服务器返回的购物车总价，单位为元。
 *
 * 返回值：
 *   0  - 添加成功，*total 已更新。
 *   -1 - total 为空、网络请求失败或服务器响应不是 OK|CART|总价 格式。
 */
int order_client_add(order_client_t *client, int dish_id, float *total)
{
    char command[32];
    char response[ORDER_CLIENT_RESPONSE_SIZE];

    if(total == NULL || snprintf(command, sizeof(command), "ADD|%d\n", dish_id) < 0 ||
       request(client, command, response, sizeof(response), 0) != 0) {
        return -1;
    }
    // sscanf 解析服务器返回的响应字符串，提取总价信息
    return sscanf(response, "OK|CART|%f", total) == 1 ? 0 : -1;
}

/**
 * 函数功能：向服务器发送 REMOVE 命令，从当前客户端的购物车删除一份指定菜品。
 *
 * 如果服务器中该菜品数量大于 1，服务器只减少数量；如果数量为 1，服务器会删除该菜品。
 *
 * 参数：
 *   client  - 已连接服务器的客户端结构体。
 *   dish_id - 要删除的菜品 ID。
 *   total   - 输出参数，成功时写入服务器返回的最新购物车总价，单位为元。
 *
 * 返回值：
 *   0  - 删除成功，*total 已更新。
 *   -1 - total 为空、网络请求失败或服务器返回错误响应。
 */
int order_client_remove(order_client_t *client, int dish_id, float *total)
{
    char command[32];
    char response[ORDER_CLIENT_RESPONSE_SIZE];

    if(total == NULL || snprintf(command, sizeof(command), "REMOVE|%d\n", dish_id) < 0 ||
       request(client, command, response, sizeof(response), 0) != 0) {
        return -1;
    }

    return sscanf(response, "OK|CART|%f", total) == 1 ? 0 : -1;
}

/**
 * 函数功能：向服务器发送 SUBMIT 命令，提交当前购物车中的订单。
 *
 * 服务器成功后会把订单写入 orders.txt，返回订单号和总价，并清空该客户端的服务端购物车。
 *
 * 参数：
 *   client   - 已连接服务器的客户端结构体。
 *   order_id - 输出参数，成功时写入服务器分配的订单号。
 *   total    - 输出参数，成功时写入订单总价，单位为元。
 *
 * 返回值：
 *   0  - 提交成功，*order_id 和 *total 已更新。
 *   -1 - 输出参数为空、网络请求失败或服务器返回错误响应。
 */
int order_client_submit(order_client_t *client, unsigned int *order_id, float *total)
{
    char response[ORDER_CLIENT_RESPONSE_SIZE];

    if(order_id == NULL || total == NULL ||
       request(client, "SUBMIT\n", response, sizeof(response), 0) != 0) {
        return -1;
    }

    return sscanf(response, "OK|ORDER|%u|%f", order_id, total) == 2 ? 0 : -1;
}
