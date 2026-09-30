/**
 ****************************************************************************************************
 * @file        lv_pic_demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       相册
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

#ifndef __LV_PIC_DEMO_H
#define __LV_PIC_DEMO_H

#include "lvgl.h"
#include "ff.h"
#include "lcd.h"
#include "app_ui.h"
#include "exfuns.h"
#include "ff.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "lvgl_demo.h"
#include "bmp.h"
#include "jpeg.h"
#include "png.h"
#include "sdmmc_cmd.h"


/* PIC状态 */
enum PIC_STATE
{
    PIC_NULL,
    PIC_PAUSE,
    PIC_PLAY,
    PIC_NEXT,
    PIC_PREV
};

/* 函数声明 */
void lv_pic_demo(void);

#endif
