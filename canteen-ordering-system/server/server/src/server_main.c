/*
 * epoll 多客户端点餐服务器。
 *
 *   socket -> bind -> listen -> epoll_create -> epoll_ctl -> epoll_wait
 *
 * 与普通单客户端服务器的区别是：
 *   1. 监听 fd 和所有客户端 fd 都加入 epoll。
 *   2. 哪个 fd 有事件，就只处理哪个 fd。
 *   3. 每个客户端 fd 对应一个 order_service_t，因此拥有独立购物车。
 */

#include "order_protocol.h"
#include "order_service.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

/* 本阶段项目最多同时让 16 个开发板或测试客户端连接 */
#define ORDER_MAX_CLIENTS 16

/*
 * 一个客户端会话。
 * fd 是该客户端的 Socket 文件描述符；service 内部保存该客户端的菜单和购物车
 */
typedef struct {
    int fd;
    order_service_t service;
} client_session_t;

/* 功能：在客户端数组中，根据 fd 找到对应的购物车会话 
*  参数：clients 是客户端数组（命令字符串）
        fd 是客户端的 Socket 文件描述符
*
*/
static client_session_t *find_client(client_session_t clients[], int fd)
{
    int index;
    // 从数组的开头开始遍历，直到找到fd对应的购物车会话
    for (index = 0; index < ORDER_MAX_CLIENTS; index++) {
        if (clients[index].fd == fd) {
            return &clients[index];
        }
    }
    return NULL;
}

/* 找到一个空闲位置，供新连接的客户端保存会话数据 */
static client_session_t *find_empty_client(client_session_t clients[])
{
    int index;
    /*从数组的开头开始遍历，直到找到一个空闲的位置*/
    for (index = 0; index < ORDER_MAX_CLIENTS; index++) {
        if (clients[index].fd == -1) {
            return &clients[index];
        }
    }
    return NULL;
}

/*
 * 客户端断开时的统一清理函数
 * 先从 epoll 中移除，再释放购物车链表，最后关闭 Socket
 */
static void remove_client(int epoll_fd, client_session_t *client)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
    order_service_destroy(&client->service);
    close(client->fd);
    client->fd = -1;
}

int main(int argc, char *argv[])
{   
    /*读取菜单文件和订单文件*/
    const char *menu_path = argc > 1 ? argv[1] : "data/menu.txt";
    const char *orders_path = argc > 2 ? argv[2] : "data/orders.txt";
    // 初始化订单服务
    int listen_fd;
    int epoll_fd;
    int option = 1; // 作用：允许复用端口
    int index;
    struct sockaddr_in address = {0};
    struct epoll_event event;
    struct epoll_event events[ORDER_MAX_CLIENTS + 1];
    client_session_t clients[ORDER_MAX_CLIENTS];

    /* 先把所有会话位置标记为空闲 */
    for (index = 0; index < ORDER_MAX_CLIENTS; index++) {
        clients[index].fd = -1;
    }

    /* 服务端启动时只重置一次共享订单号，所有客户端共同递增 */
    order_service_reset_order_number();

    /* 1. 创建 TCP 监听 Socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket error");
        return EXIT_FAILURE;
    }

    /* 服务端重启时允许复用端口，避免端口短时间处于占用状态 */
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    /* 2. 绑定端口。INADDR_ANY 表示监听阿里云服务器的所有网卡 */
    address.sin_family = AF_INET;
    address.sin_port = htons(ORDER_SERVER_PORT); // 18080 是阿里云服务器的端口
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind error");
        close(listen_fd);
        return EXIT_FAILURE;
    }
    printf("服务器端口绑定成功\n");

    /* 3. 开始监听客户端连接请求 */
    if (listen(listen_fd, ORDER_MAX_CLIENTS) == -1) {
        perror("listen error");
        close(listen_fd);
        return EXIT_FAILURE;
    }
    printf("服务器监听端口：%d\n", ORDER_SERVER_PORT);

    /* 4. 创建 epoll 句柄，并将监听 fd 加入 epoll */
    epoll_fd = epoll_create(ORDER_MAX_CLIENTS + 1);
    if (epoll_fd == -1) {
        perror("epoll_create error");
        close(listen_fd);
        return EXIT_FAILURE;
    }
    // 将监听 fd 加入 epoll，后续才能收到它发送的连接请求。
    // 事件类型为 EPOLLIN，表示可读事件。事件数据为 listen_fd。
    // 如果 epoll_ctl 返回 -1，表示发生错误，退出循环。
    // 如果 epoll_ctl 返回 0，表示成功。
    event.events = EPOLLIN;
    event.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
        perror("epoll_ctl add listen_fd error");
        close(epoll_fd);
        close(listen_fd);
        return EXIT_FAILURE;
    }

    printf("等待多个开发板客户端连接...\n");

    /* 5. epoll_wait 会等待任意一个 fd 发生可读事件 */
    while (1) {
        int ready_count = epoll_wait(epoll_fd, events, ORDER_MAX_CLIENTS + 1, -1);
        // 如果 epoll_wait 返回 -1，表示发生错误，退出循环
        if (ready_count == -1) {
            perror("epoll_wait error");
            break;
        }

        /* 逐个处理本次有事件的 fd */
        for (index = 0; index < ready_count; index++) {
            int active_fd = events[index].data.fd;

            /* 监听 fd 有事件，说明有新的客户端要连接 */
            if (active_fd == listen_fd) {
                int client_fd;
                client_session_t *client;

                client_fd = accept(listen_fd, NULL, NULL);
                if (client_fd == -1) {
                    perror("accept error");
                    continue;
                }

                /* 为该客户端分配独立购物车 */
                client = find_empty_client(clients);
                if (client == NULL) {
                    printf("客户端数量已满，拒绝新连接\n");
                    close(client_fd);
                    continue;
                }
                if (order_service_init(&client->service, menu_path, orders_path) != 0) {
                    printf("客户端订单服务初始化失败\n");
                    close(client_fd);
                    continue;
                }
                client->fd = client_fd;

                /* 将新客户端 fd 加入 epoll，后续才能收到它发送的点餐命令 */
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("新客户端 fd=%d 加入 epoll 失败\n");
                    order_service_destroy(&client->service);
                    close(client_fd);
                    client->fd = -1;
                    continue;
                }
                printf("客户端 fd=%d 连接成功\n", client_fd);
            } else {
                /* 客户端 fd 有事件，接收一条点餐命令并返回结果。 */
                client_session_t *client = find_client(clients, active_fd);
                char request[ORDER_MAX_LINE];
                char response[ORDER_MAX_LINE];
                ssize_t received;

                if (client == NULL) {
                    continue;
                }

                /*
                 * epoll 已通知该 fd 可读，因此 recv 不会无缘无故等待。
                 * 客户端每次 send 一条短命令，例如 ADD|1\n 或 CART\n。
                 */
                received = recv(active_fd, request, sizeof(request) - 1, 0);
                if (received <= 0) {
                    printf("客户端 fd=%d 断开连接\n", active_fd);
                    remove_client(epoll_fd, client);
                    continue;
                }

                /* recv 不会自动添加字符串结束符，所以必须手动补上 \0。 */
                request[received] = '\0';
                printf("客户端 fd=%d:%s", active_fd, request);
                // 处理点餐命令，返回结果。如果处理失败，返回错误信息。否则返回成功信息。例如：
                // ADD|1\n -> CART\n
                if (order_service_handle(&client->service, request, response, sizeof(response)) != 0) {
                    snprintf(response, sizeof(response), "ERROR|SERVER\n");
                }

                /* 将该客户端自己的购物车结果发回给它。 */
                //返回的信息格式：OK|CART|价格
                send(active_fd, response, strlen(response), 0);
            }
        }
    }

    close(epoll_fd);
    close(listen_fd);
    return EXIT_SUCCESS;
}
