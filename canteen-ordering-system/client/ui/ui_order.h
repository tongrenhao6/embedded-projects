#ifndef UI_ORDER_H
#define UI_ORDER_H

#include "lvgl/lvgl.h"

/*
 * 功能：在 SquareLine 创建的点餐屏幕上创建分类、菜品、购物车和结算控件。
 * 参数：screen 为点餐页面的根对象 ui_Screen1。
 * 返回值：无。
 */
void ui_order_screen_create(lv_obj_t *screen);

/*
 * 功能：连接阿里云服务器并读取 MENU 菜单，更新点餐页面。
 * 参数：无。
 * 返回值：无；连接状态显示在页面右上角。
 */
void ui_order_connect_server(void);

#endif
