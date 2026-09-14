/**
 ****************************************************************************************************
 * @file        manage.c
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

#include "manage.h"


atk_list_node_t test_list_head = ATK_LIST_INIT(test_list_head);
uint16_t test_status = 0x00;  /* 记录测试状态 */

/**
 * @brief       创建测试项目
 * @param       pfunc：测试函数入口
 * @retval      返回测试项目控制块
 */
Test_Typedef *test_create(char name[],int (*pfunc)(Test_Typedef * obj))
{
    static int i = 0;
    Test_Typedef * obj = NULL;

    obj = malloc(sizeof(Test_Typedef));                     /* 申请测试实验控制块内存 */
    obj->label = i ++;                                      /* 每一个控制块赋予标号 */
    obj->Function = pfunc;                                  /* 指向测试函数 */
    obj->name_test = name;                                  /* 实验名称 */
    atk_list_add_tail(&test_list_head,&obj->test_list_node);/* 对象尾部插入列表 */
    return obj;                                             /* 返回测试函数控制块 */
}

/**

 * @brief       运行测试项目
 * @param       无
 * @retval      无
 */
void test_handler(void)
{
    int status = 0;
    Test_Typedef *data;
    Test_Typedef *data_temp;
    data = (Test_Typedef *)malloc(sizeof(Test_Typedef));                /* 申请控件内存 */

    /* 遍历测试项目链表 */
    atk_list_for_each_entry_safe(data, data_temp, &test_list_head, Test_Typedef, test_list_node)
    {
        status = data->Function(data);                  /* 执行测试代码 */

        if (status == TEST_FAIL)
        {
            test_status |= (1 << data->label);          /* 记录测试失败 */
        }
    }

    printf("0x%x\r\n",test_status);
}