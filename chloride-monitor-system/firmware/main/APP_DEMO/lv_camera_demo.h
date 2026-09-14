/**
 ****************************************************************************************************
 * @file        lv_camera_demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       摄像头实验
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

#ifndef __LV_CAMERA_DEMO_H
#define __LV_CAMERA_DEMO_H

#include "lvgl.h"
#include "ff.h"
#include "lcd.h"
#include "app_ui.h"
#include "camera.h"
#include "esp_camera.h"
#include "mjpeg.h"
#include "lv_file_demo.h"


typedef struct
{
    lv_obj_t *lv_camera_cont;   /* 摄像头主容器 */

    struct
    {
        lv_obj_t* camera_header;
    }camera_buf;

}lv_camera_struct;

/* 函数声明 */
void lv_camera_demo(void);

#endif
