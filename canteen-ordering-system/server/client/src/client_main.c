/*
 * 终端测试客户端。
 *
 * 该程序用于在没有 LVGL 界面时测试阿里云服务器的 TCP 协议。
 * 真正运行在 6818 开发板上的 LVGL 客户端代码位于：
 * ../../lv_port_linux-v8.2/ui/order_client.c
 */

#include "client_main.h"

#define SERVER_IP "118.31.185.167"
#define SERVER_PORT 18080
#define RESPONSE_SIZE 1024

/*
 * 功能：读取一个整数；输入不是整数时提示用户重新输入。
 * 参数：prompt 为输入前显示的提示文字。
 * 返回值：返回用户输入的整数。
 */
static int read_number(const char *prompt)
{
    int value;
    int ch;

    while(1) {
        printf("%s", prompt);

        if(scanf("%d", &value) == 1) {
            return value;
        }

        printf("输入错误，请输入数字。\n");
        while((ch = getchar()) != '\n' && ch != EOF) {
            /* 清除错误输入，避免 scanf 一直读取同一个错误字符。 */
        }
    }
}

/*
 * 功能：将完整命令发送给服务器。
 * 参数：sockfd 为已连接的 socket；command 为带换行符的协议命令。
 * 返回值：成功返回 0，失败返回 -1。
 */
static int send_to_server(int sockfd, const char *command)
{
    size_t length = strlen(command);
    size_t sent = 0;

    while(sent < length) {
        ssize_t result = send(sockfd, command + sent, length - sent, 0);

        if(result <= 0) {
            perror("send error");
            return -1;
        }
        sent += (size_t)result;
    }

    return 0;
}

/*
 * 功能：接收一条普通服务器响应。
 * 参数：menu_sockfd 为已连接 socket；buffer 为接收缓冲区；len 为缓冲区大小。
 * 返回值：大于 0 表示接收字节数；0 表示服务器断开；-1 表示接收失败。
 */
int receive_from_server(int menu_sockfd, char *buffer, int len)
{
    size_t used = 0;

    if(buffer == NULL || len <= 1) {
        return -1;
    }

    while(used < (size_t)len - 1) {
        ssize_t recv_len = recv(menu_sockfd, buffer + used, (size_t)len - used - 1, 0);

        if(recv_len < 0) {
            perror("recv error");
            return -1;
        }

        if(recv_len == 0) {
            buffer[used] = '\0';
            printf("服务器已断开连接\n");
            return used == 0 ? 0 : -1;
        }

        used += (size_t)recv_len;
        buffer[used] = '\0';

        /* 普通响应只有一行，读到换行符后说明响应完整。 */
        if(strchr(buffer, '\n') != NULL) {
            return (int)used;
        }
    }

    printf("服务器响应过长，接收缓冲区不足。\n");
    return -1;
}

/*
 * 功能：接收 MENU 命令的多行响应。
 * MENU 的最后一行固定为 OK|MENU，因此必须等到它出现才算接收完成。
 * 参数：sockfd 为已连接 socket；buffer 为接收缓冲区；len 为缓冲区大小。
 * 返回值：成功返回 0，失败或缓冲区不足返回 -1。
 */
static int receive_menu_from_server(int sockfd, char *buffer, size_t len)
{
    size_t used = 0;

    if(buffer == NULL || len <= 1) {
        return -1;
    }

    while(used < len - 1) {
        ssize_t result = recv(sockfd, buffer + used, len - used - 1, 0);

        if(result <= 0) {
            if(result < 0) {
                perror("recv error");
            }
            return -1;
        }

        used += (size_t)result;
        buffer[used] = '\0';

        if(strstr(buffer, "OK|MENU\n") != NULL) {
            return 0;
        }
    }

    printf("菜单内容过长，接收缓冲区不足。\n");
    return -1;
}

/*
 * 功能：发送一条命令，并打印服务器回复。
 * 参数：sockfd 为已连接 socket；command 为命令；is_menu 为 1 时按多行菜单接收。
 * 返回值：成功返回 0，发送或接收失败返回 -1。
 */
static int send_request_and_print(int sockfd, const char *command, int is_menu)
{
    char buffer[RESPONSE_SIZE];
    int result;

    if(send_to_server(sockfd, command) != 0) {
        return -1;
    }

    if(is_menu) {
        result = receive_menu_from_server(sockfd, buffer, sizeof(buffer));
    } else {
        result = receive_from_server(sockfd, buffer, sizeof(buffer));
        if(result > 0) {
            result = 0;
        }
    }

    if(result != 0) {
        return -1;
    }

    printf("服务器响应：\n%s", buffer);
    return 0;
}

/*
 * 功能：创建 TCP socket 并连接阿里云服务器。
 * 参数：无，服务器 IP 和端口由本文件顶部的宏定义指定。
 * 返回值：成功返回 socket 文件描述符，失败返回 -1。
 */
int connect_to_server(void)
{
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket error");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if(inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) != 1) {
        perror("inet_pton error");
        close(sockfd);
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect error");
        close(sockfd);
        return -1;
    }

    printf("连接服务器成功：%s:%d\n", SERVER_IP, SERVER_PORT);
    return sockfd;
}

/*
 * 功能：按分类显示菜品，并把用户选择转换为服务器使用的菜品 ID。
 * 参数：无。
 * 返回值：成功返回菜品 ID（1 到 12）；用户取消或输入错误返回 -1。
 */
static int choose_dish_id(void)
{
    int category;
    int choice;

    printf("\n请选择菜品分类：\n");
    printf("1. 主食\n");
    printf("2. 小吃\n");
    printf("3. 饮品\n");
    printf("4. 甜品\n");
    printf("0. 返回\n");
    category = read_number("请输入分类：");

    if(category == 0) {
        return -1;
    }

    if(category == 1) {
        printf("1. 鸡腿饭  12.00 元\n2. 牛肉面  15.00 元\n3. 猪脚饭  18.00 元\n");
    } else if(category == 2) {
        printf("1. 炸鸡  8.00 元\n2. 薯条  5.00 元\n3. 烤串  3.00 元\n");
    } else if(category == 3) {
        printf("1. 矿泉水  2.00 元\n2. 柠檬茶  5.00 元\n3. 热牛奶  4.00 元\n");
    } else if(category == 4) {
        printf("1. 泡芙  3.50 元\n2. 布丁  2.00 元\n3. 蛋糕  4.00 元\n");
    } else {
        printf("没有这个分类。\n");
        return -1;
    }

    choice = read_number("请输入菜品编号：");
    if(choice < 1 || choice > 3) {
        printf("没有这个菜品。\n");
        return -1;
    }

    return (category - 1) * 3 + choice;
}

/*
 * 功能：选择一道菜并向服务器发送 ADD|菜品ID 命令。
 * 参数：menu_sockfd 为已连接服务器的 socket。
 * 返回值：成功返回 0，取消选择或网络失败返回 -1。
 */
static int add_menu(int menu_sockfd)
{
    char command[32];
    int dish_id = choose_dish_id();

    if(dish_id < 0) {
        return -1;
    }

    snprintf(command, sizeof(command), "ADD|%d\n", dish_id);
    return send_request_and_print(menu_sockfd, command, 0);
}

/*
 * 功能：读取要删除的菜品 ID，并向服务器发送 REMOVE|菜品ID 命令。
 * 参数：menu_sockfd 为已连接服务器的 socket。
 * 返回值：成功返回 0，输入错误或网络失败返回 -1。
 */
static int remove_menu(int menu_sockfd)
{
    char command[32];
    int dish_id = read_number("请输入要删除的菜品 ID（1-12）：");

    if(dish_id < 1 || dish_id > 12) {
        printf("菜品 ID 必须在 1 到 12 之间。\n");
        return -1;
    }

    snprintf(command, sizeof(command), "REMOVE|%d\n", dish_id);
    return send_request_and_print(menu_sockfd, command, 0);
}

/*
 * 功能：显示终端测试菜单，循环处理查看菜单、添加、删除、查看购物车和结算操作。
 * 参数：menu_sockfd 为已经连接服务器的 socket。
 * 返回值：无；用户选择退出后函数返回。
 */
void show_menu(int menu_sockfd)
{
    int choice;

    printf("欢迎使用云嵌食堂校园点餐平台\n");

    while(1) {
        printf("\n====================\n");
        printf("1. 查看菜单\n");
        printf("2. 添加菜品\n");
        printf("3. 删除菜品\n");
        printf("4. 查看购物车\n");
        printf("5. 提交订单\n");
        printf("6. 退出\n");
        printf("====================\n");

        choice = read_number("请输入您的选择：");

        switch(choice) {
        case 1:
            send_request_and_print(menu_sockfd, "MENU\n", 1);
            break;
        case 2:
            add_menu(menu_sockfd);
            break;
        case 3:
            remove_menu(menu_sockfd);
            break;
        case 4:
            send_request_and_print(menu_sockfd, "CART\n", 0);
            break;
        case 5:
            send_request_and_print(menu_sockfd, "SUBMIT\n", 0);
            break;
        case 6:
            close(menu_sockfd);
            printf("客户端已退出。\n");
            return;
        default:
            printf("请输入 1 到 6。\n");
            break;
        }
    }
}

/*
 * 功能：终端测试客户端程序入口。
 * 参数：无。
 * 返回值：0 表示正常退出；-1 表示连接服务器失败。
 */
int main(void)
{
    int sockfd = connect_to_server();

    if(sockfd < 0) {
        return -1;
    }

    show_menu(sockfd);
    return 0;
}
