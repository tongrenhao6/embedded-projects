/*
    本文件用于处理菜单数据
*/

#include "menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 加载菜单数据，并存储在menu_t结构体中
// 参数：
//     menu：菜单结构体指针
//     path：菜单数据文件路径
// 返回值：
//     0：成功
//     -1：失败
// 注意事项：
//     1. 菜单数据文件格式为：id|category|name|price，price单位为元
//     2. 菜单数据文件最多包含64个菜品
int menu_load(menu_t *menu, const char *path)
{
    FILE *file;
    char line[192]; // 用于存储读取的每一行数据
    if (menu == NULL || path == NULL) {
        return -1;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    menu->count = 0;  // 初始化菜品数量为0
    // 读取每一行数据，并解析菜品信息
    // fgets函数会读取一行数据，并将数据存储在line数组中，直到遇到换行符或文件结束
    while (fgets(line, sizeof(line), file) != NULL && menu->count < 64) {
        dish_t *dish;
        char *id;
        char *category;
        char *name;
        char *price;
        // 如果当前行是注释行或空行，则跳过该行
        // 如果当前行的长度为0，则跳过该行
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        // 使用strtok函数将每一行数据分割成id、category、name、price四个部分
        // strtok函数会从line数组中找到第一个分隔符（即'|'），并将line数组的指针移动到分隔符之后的位置
        // 菜单文件格式为：id|category|name|price
        id = strtok(line, "|\r\n");
        category = strtok(NULL, "|\r\n");
        name = strtok(NULL, "|\r\n");
        price = strtok(NULL, "|\r\n");
        // 如果id、category、name、price任何一个部分为空，则跳过该行，防止使用不完整的菜品数据
        if (id == NULL || category == NULL || name == NULL || price == NULL) {
            continue;
        }
        
        dish = &menu->dishes[menu->count];  // 获取当前菜品的指针（menu的结构体数组的指针，还定义了大小）
        dish->id = atoi(id);  // 将id字符串转换为整数，并存储在dish的id字段中
        // 使用snprintf函数将category、name字符串复制到dish的category、name字段中
        snprintf(dish->category, sizeof(dish->category), "%s", category);
        snprintf(dish->name, sizeof(dish->name), "%s", name);
        // 使用atof函数将price字符串转换为float类型，并存储在dish的price字段中
        dish->price = (float)atof(price); 
        menu->count++;  // 增加菜品数量
    }

    fclose(file);
    return 0;
}

// 找到菜品ID对应的菜品信息
// 参数：
//     menu：菜单结构体指针
//     dish_id：菜品ID
// 返回值：
//     菜品信息指针，如果菜品ID不存在，则返回NULL
// 注意事项：
//     1. 菜品ID从1开始，最大为64
//     2. 菜品信息存储在menu_t结构体的dishes数组中
const dish_t *menu_find(const menu_t *menu, int dish_id)
{
    size_t index;  // 定义一个变量，用于存储菜品ID对应的索引

    if (menu == NULL) {
        return NULL;
    }
    // 遍历menu_t结构体的dishes数组，找到菜品ID对应的索引
    // 如果找到菜品ID对应的索引，则返回该索引对应的菜品信息指针
    // 如果遍历完数组都没有找到菜品ID对应的索引，则返回NULL
    for (index = 0; index < menu->count; index++) {
        if (menu->dishes[index].id == dish_id) {
            return &menu->dishes[index];
        }
    }

    return NULL;
}
