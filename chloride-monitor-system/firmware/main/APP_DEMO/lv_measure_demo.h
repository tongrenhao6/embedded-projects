/**
 ****************************************************************************************************
 * @file        lv_measure_demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       测量系统
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台: 正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __LV_MEASURE_DEMO_H
#define __LV_MEASURE_DEMO_H

#include "lvgl.h"
#include "ff.h"
#include "lcd.h"
#include "app_ui.h"
#include "ph_sensor_rs485.h"


/* 测量结构体 */
typedef struct
{
    lv_obj_t *lv_main_cont;
    
   struct
   {
        lv_obj_t *lv_small_cont;
        struct
        {
            lv_timer_t * lv_ap3216c_timer;
            lv_obj_t * ap_ir;
            lv_obj_t * ap_ps;
            lv_obj_t * ap_als;
            lv_obj_t * ap_ir_label;
            lv_obj_t * ap_ir_unit;
            lv_obj_t * ap_ps_label;
            lv_obj_t * ap_ps_unit;
            lv_obj_t * ap_als_label;
            lv_obj_t * ap_als_unit;
        }ap3216c;
            
        struct
        {
            lv_timer_t * lv_temp_timer;
            lv_obj_t * temp_obj;
            lv_obj_t * temp_label;
            lv_obj_t * temp_unit;
            lv_obj_t * temp_slider;
            lv_obj_t * temp_line1;
            lv_obj_t * temp_line1_1;
            lv_obj_t * temp_line2;
            lv_obj_t * temp_line2_1;
        }temp;
   }small_cont;
}measure_obj_t;

/* 函数声明 */
void lv_measure_demo(void);

#endif
