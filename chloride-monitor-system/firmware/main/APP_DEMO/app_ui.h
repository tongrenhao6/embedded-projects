/**
 ****************************************************************************************************
 * @file        app_ui.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-01
 * @brief       ESP32-S3综合例程UI
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

#ifndef __APP_DEMO_H
#define __APP_DEMO_H

#include "xl9555.h"
#include "manage.h"
#include "lcd.h"
#include "led.h"
#include "key.h"
#include "24cxx.h"
#include "adc1.h"
#include "ap3216c.h"
#include "spi_sdcard.h"
#include "qma6100p.h"
#include "tinyusb.h"
#include "lvgl.h"
#include "image.h"
#include "lv_file_demo.h"
#include "lv_clock_demo.h"
#include "lv_measure_demo.h"
#include "lv_data_demo.h"
#include "esp_rtc.h"


#define APP_NUM    9

/* APP图标描述信息 */
typedef struct
{
    char* app_text_English;
    char* app_text_Chinese;
    const void* app_image;
}app_image_info;

/* APP图标描述信息 */
typedef struct
{
    uint8_t image_flag;
    lv_img_dsc_t image_bin[APP_NUM];
    lv_obj_t *background;
    lv_img_dsc_t image_background_bin;
    lv_obj_t *app_btn[APP_NUM];
    lv_obj_t *app_name[APP_NUM];
    lv_obj_t *lv_main_cont;
    
   struct
   {
        lv_timer_t* lv_rtc_timer;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint8_t year;
        uint8_t month;
        uint8_t date;
        uint8_t week;
        char rtc_tbuf[40];
   }rtc;
    
}app_obj_t;

/* 返回控制器（可用在返回主界面） */
typedef struct
{
    lv_obj_t* current_parent;
    lv_obj_t* fouc_parent;
    lv_obj_t* del_parent;
    void (*Function)(void);     /* 删除界面所有控件函数 */
} lv_m_general;

extern lv_m_general app_obj_general;
extern app_obj_t app_obj_btn;
extern lv_group_t *ctrl_g;
/* 函数声明 */
void lv_load_main_window(void);
void lv_msgbox(char *name);
void lv_app_show(void);
void lv_app_del(void);
FRESULT atk_dir_sdi(FF_DIR *dp, DWORD ofs);
#endif
