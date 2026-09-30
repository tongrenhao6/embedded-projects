#ifndef CART_H
#define CART_H

#include "menu.h"
// 购物车结构体
typedef struct cart_item {
    dish_t dish;
    int quantity;
    struct cart_item *next;
} cart_item_t;

typedef struct {
    cart_item_t *head;
} cart_t;

void cart_init(cart_t *cart);
int cart_add(cart_t *cart, const dish_t *dish, int quantity);
int cart_remove(cart_t *cart, int dish_id);
float cart_total_price(const cart_t *cart);
void cart_clear(cart_t *cart);

#endif
