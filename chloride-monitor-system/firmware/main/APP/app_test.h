/**
 ****************************************************************************************************
 * @file        app_test.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-01
 * @brief       APP功能测试
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

#ifndef __APP_TEST_H
#define __APP_TEST_H

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
#include "es8388.h"
#include "i2s.h"
#include <inttypes.h>
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "tusb_tasks.h"
#include "emission.h"
#include "manage.h"


/* USB控制器 */
typedef struct
{
    uint8_t status;                         /* bit0:0,断开;1,连接 */
}__usbdev;

/* 函数声明 */
int led_test(Test_Typedef * obj);
int key_test(Test_Typedef * obj);
int beep_test(Test_Typedef * obj);
int at24cx_test(Test_Typedef * obj);
int adc_test(Test_Typedef * obj);
int ap3216c_test(Test_Typedef * obj);
int sd_test(Test_Typedef * obj);
int qma6100p_test(Test_Typedef * obj);
int es8388_test(Test_Typedef * obj);
int usb_test(Test_Typedef * obj);
int rmt_test(Test_Typedef * obj);
void func_test(void);
#endif