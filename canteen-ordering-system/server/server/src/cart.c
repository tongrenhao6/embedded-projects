/*
    本文件实现了购物车的管理功能，包括初始化、添加、移除、计算总价和清空购物车等功能。
    购物车使用链表结构来存储购物车项，每个购物车项包含菜品信息和数量。
    购物车项使用结构体 cart_item_t 来存储菜品信息和数量。购物车使用头指针来表示链表的头节点。
    购物车提供初始化、添加、移除、计算总价和清空购物车等接口，以方便用户进行购物车操作。
*/

#include "cart.h"

#include <stdlib.h>
// 购物车初始化
void cart_init(cart_t *cart)
{
    if (cart != NULL) {
        cart->head = NULL;
    }
}
// 购物车添加菜品
//参数：cart：购物车指针，
//      dish：菜品指针，
//      quantity：菜品数量
//返回值：0表示成功，-1表示失败
int cart_add(cart_t *cart, const dish_t *dish, int quantity)
{
    cart_item_t *item;  // 创建一个新的购物车项
    // 检查参数是否合法
    if (cart == NULL || dish == NULL || quantity <= 0) {
        return -1;
    }
    // 遍历购物车，检查是否已经存在该菜品，存在则增加数量
    for (item = cart->head; item != NULL; item = item->next) {
        if (item->dish.id == dish->id) {
            item->quantity += quantity;
            return 0;
        }
    }
    // 如果不存在，则创建一个新的购物车项并添加到链表的头部
    item = calloc(1, sizeof(*item));
    if (item == NULL) {
        return -1;
    }
    // 初始化购物车项
    item->dish = *dish;
    item->quantity = quantity;
    item->next = cart->head;
    cart->head = item;
    return 0;
}

// 购物车移除一份菜品
//参数：cart：购物车指针，
//      dish_id：菜品ID
//返回值：0表示成功，-1表示失败
int cart_remove(cart_t *cart, int dish_id)
{
    cart_item_t *current;          // 当前节点
    cart_item_t *previous = NULL;  // 上一个节点

    if (cart == NULL) {
        return -1;
    }
    // 遍历购物车，检查是否已经存在该菜品，存在则减少一份
    for (current = cart->head; current != NULL; current = current->next) {
        if (current->dish.id == dish_id) {
            // 数量大于1时，-按钮只减少一份，节点仍然保留在购物车中
            if (current->quantity > 1) {
                current->quantity--;
                return 0;
            }
            // 数量正好为1时，减少后数量为0，需要将该节点从链表中删除
            if (previous == NULL) {
                cart->head = current->next; 
            } else {
                previous->next = current->next; // 移除菜品
            }
            free(current);
            return 0;
        }
        previous = current;
    }

    return -1;
}

// 购物车计算总价
//参数：cart：购物车指针
//返回值：总价（以元作为单位）
float cart_total_price(const cart_t *cart)
{
    const cart_item_t *item;  
    float total = 0.0f;

    if (cart == NULL) {
        return 0;
    }
    // 遍历购物车，计算总价
    for (item = cart->head; item != NULL; item = item->next) {
        total += item->dish.price * item->quantity;// 总价=菜品价格*菜品数量，单位为元
    }

    return total;
}

// 购物车清空购物车
//参数：cart：购物车指针
//返回值：无
void cart_clear(cart_t *cart)
{
    cart_item_t *item;

    if (cart == NULL) {
        return;
    }

    item = cart->head;
    while (item != NULL) {
        cart_item_t *next = item->next;
        free(item);
        item = next;
    }
    cart->head = NULL;
}
