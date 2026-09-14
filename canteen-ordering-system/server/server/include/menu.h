#ifndef MENU_H
#define MENU_H

#include <stddef.h>
// 菜品结构体
typedef struct {
    int id;
    char category[32];
    char name[64];
    float price;  // 菜品价格，单位是元，例如12.0表示12元
} dish_t;
// 菜单结构体，包含菜品结构体数组和菜品数量
typedef struct {
    dish_t dishes[64];
    size_t count;
} menu_t;

int menu_load(menu_t *menu, const char *path);
const dish_t *menu_find(const menu_t *menu, int dish_id);

#endif
