/*
 * 订单业务层。
 *
 * server_main.c 只关心 Socket 收发：收到一行文本，发送一行文本；
 * 本文件则负责解释这行文本的业务含义，并操作菜单、购物车和订单文件。
 *
 * 客户端请求示例：
 *   ADD|1\n         把编号 1 的菜品增加 1 份
 *   REMOVE|1\n      把编号 1 的菜品减少 1 份
 *   CART\n            查询当前购物车总价
 *   SUBMIT\n          确认下单
 *
 * 服务端响应示例：
 *   OK|CART|24.00\n  购物车总价为 24.00 元
 *   ERROR|DISH\n      菜品不存在或购物车操作失败
 */

#include "order_service.h" /* menu_t、cart_t 和对外函数声明 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 所有客户端共用同一个订单号。epoll 服务器在一个线程中处理事件，
 * 所以本项目不需要再使用互斥锁保护这个变量。
 */
static unsigned int next_order_id = 1;

/* 服务端启动或单元测试开始前调用，使订单号从 1 重新开始。 */
void order_service_reset_order_number(void)
{
    next_order_id = 1;
}

/*
 * 将当前购物车写入订单文件。
 *参数：service：订单服务对象
 *     order_id：订单号
 * 返回值：0 表示成功，-1 表示失败。失败时购物车不会被清空。
 * 当前实现只在订单文件打开失败时返回 -1。
 *
 * 文件使用追加模式 "a" 打开，因此每次下单都会保留原有订单，
 * 不会覆盖以前的内容。每一行的格式为：
 *   订单号|总价(元)|菜品编号,数量;菜品编号,数量
 * 例如：1|29.00|1,2;4,1
 *
 * 此函数只负责写文件，不负责清空购物车。调用者必须在写入成功后
 * 才清空购物车，否则文件写入失败时用户已选菜品会丢失。
 */
static int append_order(const order_service_t *service, unsigned int order_id)
{
    const cart_item_t *item; /* 遍历链表时使用的只读指针 */
    FILE *orders = fopen(service->orders_path, "a");

    /* 文件打不开通常表示路径错误、目录不存在或没有写权限。 */
    if (orders == NULL)
    {
        return -1;
    }

    /* 先写订单号和总价，菜品明细紧随其后。 */
    // fprintf 函数功能：格式化输出到文件
    fprintf(orders, "%u|%.2f|", order_id, cart_total_price(&service->cart));

    /*
     * cart.head 是单链表头结点。每个结点保存一种菜品及其数量。
     * 使用 item = item->next 能依次访问整张订单的所有菜品。
     */
    for (item = service->cart.head; item != NULL; item = item->next)
    {
        fprintf(orders, "%d,%d", item->dish.id, item->quantity);

        /* 最后一项后面不加分号，保持文件格式整洁。 */
        if (item->next != NULL)
        {
            fputc(';', orders);
        }
    }

    /* 一张订单写完换行，下一次下单会写到新的一行。 */
    fputc('\n', orders);  //fputc的作用是换行
    fclose(orders);
    return 0;
}

/*
 * 将内存中的菜单数组转换成网络文本响应。
 * 参数：service：订单服务对象
 *     response：响应缓冲区
 *     response_size：响应缓冲区大小
 * 返回值：0 表示成功，-1 表示失败。失败原因是响应缓冲区空间不足或格式化失败。
 *
 * 每道菜占一行：ITEM|编号|分类|名称|价格(元)\n
 * 所有菜品发送完成后，再追加 OK|MENU\n 作为菜单结束标记。
 * 客户端可据此判断菜单是否收完。
 */
static int render_menu(const order_service_t *service, char *response, size_t response_size)
{
    size_t index;    /* 菜单数组下标 */
    size_t used = 0; /* response 缓冲区已经写入的字节数 */

    for (index = 0; index < service->menu.count; index++)
    {
        const dish_t *dish = &service->menu.dishes[index];

        /*
         * response + used：从未使用的位置继续拼接下一行。
         * response_size - used：告诉 snprintf 剩余空间，防止越界。
         */
        int written = snprintf(response + used, response_size - used,
                               "ITEM|%d|%s|%s|%.2f\n", dish->id, dish->category,
                               dish->name, dish->price);

        /*
         * snprintf 返回本次本应写入的字符数。
         * 返回负数代表格式化失败；返回值大于等于剩余空间代表被截断。
         */
        if (written < 0 || (size_t)written >= response_size - used)
        {
            return -1;
        }

        /* 为下一道菜移动写入位置。 */
        used += (size_t)written;
    }

    /* 所有 ITEM 行都成功写入后，再写入菜单结束标记。 */
    return snprintf(response + used, response_size - used, "OK|MENU\n") < 0 ? -1 : 0;
}

/*
 * 初始化一个客户端对应的订单服务对象。
 * 参数：service：订单服务对象
 *     menu_path：菜品文件路径
 *     orders_path：订单文件路径
 * 返回值：0 表示成功，-1 表示失败。失败原因是参数为空、路径过长或菜单文件读取失败。
 *
 * 服务端每 accept 一个客户端连接，就会创建一个 order_service_t，
 * 因而每个客户端都有自己的购物车链表，彼此不会混在一起。
 */
int order_service_init(order_service_t *service, const char *menu_path, const char *orders_path)
{
    /*
     * 先检查指针和路径长度。orders_path 会被复制到结构体的固定数组中，
     * 所以必须保证它能完整放下，避免字符串越界。
     */
    if (service == NULL || menu_path == NULL || orders_path == NULL ||
        strlen(orders_path) >= sizeof(service->orders_path))
    {
        return -1;
    }

    /* 清零菜单计数、链表头指针和订单文件路径等所有成员。 */
    memset(service, 0, sizeof(*service));

    /* 从 menu.txt 读取菜品；读取失败则不能继续提供点餐服务。 */
    if (menu_load(&service->menu, menu_path) != 0)
    {
        return -1;
    }

    /* 初始化空购物车，明确 head 为 NULL。 */
    cart_init(&service->cart);

    /* 保存订单文件路径，供 SUBMIT 命令使用。 */
    snprintf(service->orders_path, sizeof(service->orders_path), "%s", orders_path);

    /* 订单号由本文件顶部的共享变量统一管理，不属于某一个客户端。 */
    return 0;
}

/*
 * 处理一条来自客户端的请求。
 * 参数：service：订单服务对象
 *     request：Socket 层收到的原始文本
 *     response：Socket 层发送的响应文本
 *     response_size：响应缓冲区大小
 * 返回值：0 表示成功，-1 表示失败。失败原因是参数为空或响应缓冲区大小为 0。
 *
 * request 是 Socket 层收到的原始文本，response 是由本函数生成的返回文本。
 * 约定：成功返回 0；参数或内部处理失败返回 -1。
 */
int order_service_handle(order_service_t *service, const char *request, char *response, size_t response_size)
{
    char request_copy[128]; /* strtok 会修改字符串，因此不能直接处理 request */
    char *command;          /* 请求的第一个字段，如 ADD、CART、SUBMIT */

    /*
     * request_copy 是固定大小数组，所以先验证请求长度。
     * response_size 为 0 时不能写入任何响应。
     */
    if (service == NULL || request == NULL || response == NULL || response_size == 0 ||
        strlen(request) >= sizeof(request_copy))
    {
        return -1;
    }

    /* 复制请求后再分割，确保上层 Socket 缓冲区内容不被 strtok 改写。 */
    snprintf(request_copy, sizeof(request_copy), "%s", request);

    /*
     * strtok 的第一次调用传入字符串首地址，分隔符同时包含：
     *   |    协议字段分隔符
     *   \r\n  网络消息结尾的换行符
     */
    command = strtok(request_copy, "|\r\n");// strtok 会修改字符串，把分隔符替换为 '\0'，返回第一个字段的首地址。
    if (command == NULL)
    {
        return snprintf(response, response_size, "ERROR|EMPTY\n") < 0 ? -1 : 0;
    }

    /* 客户端请求完整菜单。 */
    if (strcmp(command, "MENU") == 0)
    {
        return render_menu(service, response, response_size);
    }

    /* 客户端只查询总价，不改变购物车。 */
    if (strcmp(command, "CART") == 0)
    {
        return snprintf(response, response_size, "OK|CART|%.2f\n", cart_total_price(&service->cart)) < 0 ? -1 : 0;
    }

    /*
     * ADD 请求格式：ADD|菜品编号
     * 每次点击 LVGL 的 + 按钮只发一次 ADD，所以不需要数量字段。
     */
    if (strcmp(command, "ADD") == 0)
    {
        char *dish_id_text = strtok(NULL, "|\r\n");
        const dish_t *dish;

        /* 请求字段不完整，例如只发 ADD 时，直接返回格式错误。 */
        if (dish_id_text == NULL)
        {
            return snprintf(response, response_size, "ERROR|BAD_ADD\n") < 0 ? -1 : 0;
        }

        /* atoi 将文本 "1" 转为整数后查找菜单。 */
        dish = menu_find(&service->menu, atoi(dish_id_text));

        /* 菜品不存在，或数量非法/内存申请失败时，购物车添加会失败。 */
        if (dish == NULL || cart_add(&service->cart, dish, 1) != 0)
        {
            return snprintf(response, response_size, "ERROR|DISH\n") < 0 ? -1 : 0;
        }

        /* 添加成功后立即返回最新总价，方便 LVGL 刷新购物车区域。 */
        return snprintf(response, response_size, "OK|CART|%.2f\n", cart_total_price(&service->cart)) < 0 ? -1 : 0;
    }

    /* REMOVE 请求格式：REMOVE|菜品编号，表示从购物车链表中减少一份。 */
    if (strcmp(command, "REMOVE") == 0)
    {
        char *dish_id_text = strtok(NULL, "|\r\n");

        /* 不带编号，或链表中找不到该菜品，都视为删除失败。 */
        if (dish_id_text == NULL || cart_remove(&service->cart, atoi(dish_id_text)) != 0)
        {
            return snprintf(response, response_size, "ERROR|DISH\n") < 0 ? -1 : 0;
        }
        return snprintf(response, response_size, "OK|CART|%.2f\n", cart_total_price(&service->cart)) < 0 ? -1 : 0;
    }

    /*
     * SUBMIT 没有额外参数。
     * 执行顺序很重要：先计算金额，再写订单文件，最后清空购物车。
     */
    if (strcmp(command, "SUBMIT") == 0)
    {
        unsigned int order_id;
        float total = cart_total_price(&service->cart);

        /* 空购物车不能生成订单。 */
        if (service->cart.head == NULL)
        {
            return snprintf(response, response_size, "ERROR|EMPTY_CART\n") < 0 ? -1 : 0;
        }

        /* 取当前订单号后自增，为下一张订单预留编号。 */
        order_id = next_order_id++;

        /* 文件写入失败时保留购物车，用户可以稍后重试提交。 */
        if (append_order(service, order_id) != 0)
        {
            return snprintf(response, response_size, "ERROR|ORDER_WRITE\n") < 0 ? -1 : 0;
        }

        /* 订单已成功落盘，释放链表结点并回到空购物车状态。 */
        cart_clear(&service->cart);
        return snprintf(response, response_size, "OK|ORDER|%u|%.2f\n", order_id, total) < 0 ? -1 : 0;
    }

    /* 第一个字段不是已定义命令时，统一返回未知命令。 */
    return snprintf(response, response_size, "ERROR|COMMAND\n") < 0 ? -1 : 0;
}

/*
 * 销毁订单服务对象。
 * 参数：service：订单服务对象
 * 返回值：无。
 * 客户端断开连接时必须调用，确保购物车链表中动态申请的内存被释放。
 */
void order_service_destroy(order_service_t *service)
{
    if (service != NULL)
    {
        cart_clear(&service->cart);
    }
}
