/**
 ****************************************************************************************************
 * @file        mjpeg.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-12-01
 * @brief       MJPEG视频处理 代码
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
 
#ifndef __MJPEG_H
#define __MJPEG_H 

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jpeglib.h"
#include "lcd.h"


struct my_error_mgr
{
    struct jpeg_error_mgr pub;  /* 公共”字段 */
    jmp_buf setjmp_buffer;      /* 用于返回 */
};

extern int Windows_Width;       /* 图像宽度 */
extern int Windows_Height;      /* 图像高度 */
#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

typedef void (*lcd_write_cb)(uint32_t w,uint32_t h,uint8_t *video_buf);

/* 函数声明 */
char mjpegdec_init(uint16_t offx, uint16_t offy);
void mjpegdec_free(void);
uint8_t mjpegdec_decode(uint8_t* buf, uint32_t bsize,lcd_write_cb lcd_cb);
void mjpegdec_malloc(void);
void mjpegdec_video_free(void);
#endif
