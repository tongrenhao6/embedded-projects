/**
 ****************************************************************************************************
 * @file        manage.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-01
 * @brief       管理
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#ifndef __MANAGE_H
#define __MANAGE_H

#include "list.h"
#include <stdint.h>
#include <stddef.h>
#include "esp_timer.h"


/* 测试实验状态 */
enum STATE
{
    TEST_OK,
    TEST_FAIL
};

/* 菜单结构体 */
typedef struct Test
{
    atk_list_node_t test_list_node;     /* 父类链表的节点 */
    char *name_test;                    /* 实验名称 */
    uint8_t label;                      /* 标号 */
    int (*Function)(void * widget);     /* 测试函数 */
}Test_Typedef;

extern uint16_t test_status;            /* 导出变量 */

/* 函数声明 */
Test_Typedef *test_create(char name[],int (*pfunc)(Test_Typedef * obj));
void test_handler(void);

#endif
