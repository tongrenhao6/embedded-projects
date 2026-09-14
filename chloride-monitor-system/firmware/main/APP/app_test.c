/**
 ****************************************************************************************************
 * @file        app_test.c
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

#include "app_test.h"


/**
 * @brief       led测试
 * @param       无
 * @retval      无
 */
int led_test(Test_Typedef * obj)
{
    uint8_t key = 0;
    uint8_t xl_key = 0;
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);
    printf("\r\n/******************************* 1，LED测试 *******************************/\r\n");
    printf("/********************* 注意：观察LED0和LED1是否闪烁！***********************/\r\n");
    printf("/****************************** NEXT: BOOT ;Record: KEY0 *******************************/\r\n");

    while(1)
    {
        key = key_scan(0);
        xl_key = xl9555_key_scan(0);
        LED_TOGGLE();

        if (key == BOOT_PRES)
        {
            LED(1);
            vTaskDelay(500);
            printf("******OK!******\r\n");
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
            return TEST_OK;
        }

        if (xl_key == KEY0_PRES)
        {
            LED(1);
            printf("******FAIL!******\r\n");
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
            return TEST_FAIL;
        }

        vTaskDelay(100);
    }
}

/**
 * @brief       key测试
 * @param       无
 * @retval      无
 */
int key_test(Test_Typedef * obj)
{
    uint8_t key = 0;
    uint8_t xl_key = 0;
    uint16_t key_status = 0x00;

    printf("\r\n/******************************* 2，按键测试 *******************************/\r\n");
    printf("/********************* 按下KEY0、KEY1、KEY2、KEY3和BOOT即可退出！***********************/\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);
    lcd_draw_circle(50,lcd_self.height / 2 + 30,10,WHITE);
    lcd_draw_circle(75,lcd_self.height / 2 + 30,10,WHITE);
    lcd_draw_circle(100,lcd_self.height / 2 + 30,10,WHITE);
    lcd_draw_circle(125,lcd_self.height / 2 + 30,10,WHITE);
    lcd_draw_circle(100,lcd_self.height / 2 + 5,10,WHITE);

    while(1)
    {
        key = key_scan(0);
        xl_key = xl9555_key_scan(0);

        if (key == BOOT_PRES)
        {
            key_status |= 1 << 0;
            lcd_draw_circle(50,lcd_self.height / 2 + 30,10,RED);
            printf("按键BOOT被按下\n");
        }

        switch(xl_key)
        {
            case KEY0_PRES:
                key_status |= 1 << 1;
                lcd_draw_circle(125,lcd_self.height / 2 + 30,10,RED);
                printf("按键KEY0被按下\n");
                break;
            case KEY1_PRES:
                key_status |= 1 << 2;
                lcd_draw_circle(100,lcd_self.height / 2 + 30,10,RED);
                printf("按键KEY1被按下\n");
                break;
            case KEY2_PRES:
                key_status |= 1 << 3;
                lcd_draw_circle(75,lcd_self.height / 2 + 30,10,RED);
                printf("按键KEY2被按下\n");
                break;
            case KEY3_PRES:
                key_status |= 1 << 4;
                lcd_draw_circle(100,lcd_self.height / 2 + 5,10,RED);
                printf("按键KEY3被按下\n");
                break;
        }

        if ((key_status & 0x1F) == 0x1F)
        {
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
            lcd_fill(30,lcd_self.height / 2 - 10,150,lcd_self.height / 2 + 50,BLACK);
            key_status = 0x00;
            printf("按键全部按下，正常退出！\r\n");
            return TEST_OK;
        }

        vTaskDelay(10);
    }
}

/**
 * @brief       beep测试
 * @param       无
 * @retval      无
 */
int beep_test(Test_Typedef * obj)
{
    uint8_t key = 0;
    uint8_t xl_key = 0;
    
    printf("\r\n/******************************* 3，蜂鸣器测试 *******************************/\r\n");
    printf("/**************************** 注意：听下蜂鸣器是否鸣叫！***********************/\r\n");
    printf("/****************************** NEXT: BOOT ;Record: KEY0 *******************************/\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);

    xl9555_pin_write(BEEP_IO,0);                /* 打开蜂鸣器 */

    while(1)
    {
        key = key_scan(0);
        xl_key = xl9555_key_scan(0);

        if (key == BOOT_PRES)
        {
            printf("******OK!******\r\n");
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
            xl9555_pin_write(BEEP_IO,1);        /* 关闭蜂鸣器 */
            return TEST_OK;
        }

        if (xl_key == KEY0_PRES)
        {
            printf("******FAIL!******\r\n");
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
            xl9555_pin_write(BEEP_IO,1);        /* 关闭蜂鸣器 */
            return TEST_FAIL;
        }

        vTaskDelay(10);
    }
}

const uint8_t g_text_buf[] = {"EEPROM"};        /* 要写入到24c02的字符串数组 */
#define TEXT_SIZE   sizeof(g_text_buf)          /* TEXT字符串长度 */
uint8_t datatemp[TEXT_SIZE];

/**
 * @brief       at24c02测试
 * @param       无
 * @retval      无
 */
int at24cx_test(Test_Typedef * obj)
{
    uint8_t err = 0;
    

    printf("\r\n/******************************* 4，AT24C02 *******************************/\r\n");
    printf("/********************************** NEXT: AUTO ********************************/\r\n\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);
    err = at24cxx_check();                      /* 检测AT24C02 */

    if (err != 0)
    {
        while (1)                               /* 检测不到24c02 */
        {
            printf("24C02 check failed, please check!\n");
            lcd_show_string(40, 30 + obj->label * 16, lcd_self.width, 16, 16, "24C02 failed", WHITE);
            vTaskDelay(10);
        }
    }

    /* 写数据 */
    at24cxx_write(0, (uint8_t *)g_text_buf, TEXT_SIZE);
    vTaskDelay(100);
    /* 读数据 */
    at24cxx_read(0, datatemp, TEXT_SIZE);
    vTaskDelay(100);

    /* 匹配写和读数据是否一致 */
    if (strcmp((char *)datatemp, (char *)g_text_buf) == 0)
    {
        lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
        printf("******OK!******\r\n");
        return TEST_OK;
    }
    else
    {
        lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", WHITE);
        printf("******FAIL!******\r\n");
        return TEST_FAIL;
    }
}

/**
 * @brief       at24c02测试
 * @param       无
 * @retval      无
 */
int adc_test(Test_Typedef * obj)
{
    uint8_t key = 0;
    uint8_t xl_key = 0;
    uint16_t adcdata = 0;
    float voltage = 0;
    adc_init();     /* 初始化ADC */
    
    printf("\r\n/******************************* 5，ADC测试 *********************************/\r\n");
    printf("/*** 注意：请接上P3处的跳线帽，将AIN与RV1进行短接，然后调节RV1观看ADC读数变化！！ ***/\r\n");
    printf("******确认P3端的AIN和RV是否接上******\r\n");
    printf("/*********************************** NEXT: BOOT **********************************/\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);
    lcd_show_string(10 + 16 * 4, 30 + obj->label * 16, lcd_self.width, 16, 16,"P3:AIN>RV?",RED);      /* 显示电压值的整数部分 */

    while(1)
    {
        key = key_scan(0);

        if (key == BOOT_PRES)
        {
            break;
        }

        vTaskDelay(100);
    }
    printf("******确认P3端的AIN和RV已接上******\r\n");
    lcd_fill(10 + 16 * 4,30 + obj->label * 16,30 + obj->label * 16 + 16 * 4,30 + obj->label * 16 + 16,BLACK);
    printf("/****************************** NEXT: BOOT ;Record: KEY0 *******************************/\r\n");

    while(1)
    {
        key = key_scan(0);
        xl_key = xl9555_key_scan(0);

        if (key == BOOT_PRES)
        {
            printf("******OK!******\r\n");
            lcd_fill(10 + 16 * 4,30 + obj->label * 16,30 + obj->label * 16 + 16 * 2,30 + obj->label * 16 + 16,BLACK);
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
            return TEST_OK;
        }

        if (xl_key == KEY0_PRES)
        {
            printf("******FAIL!******\r\n");
            lcd_fill(10 + 16 * 4,30 + obj->label * 16,30 + obj->label * 16 + 16 * 2,30 + obj->label * 16 + 16,BLACK);
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
            return TEST_FAIL;
        }

        adcdata = adc_get_result_average(ADC_ADCX_CHY, 10);
        voltage = (float)adcdata * (3.3 / 4096);                /* 获取计算后的带小数的实际电压值 */
        printf("ADC:%0.2f\r\n",voltage);
        adcdata = voltage;                                      /* 赋值整数部分给adcx变量 */
        lcd_show_xnum(10 + 16 * 4, 30 + obj->label * 16, adcdata, 1, 16, 0, WHITE);      /* 显示电压值的整数部分 */
        lcd_show_char(10 + 16 * 4 + 8, 30 + obj->label * 16,'.',16,0, WHITE);
        voltage -= adcdata;                                     /* 把已经显示的整数部分去掉，留下小数部分 */
        voltage *= 1000;                                        /* 小数部分乘以1000 */
        lcd_show_xnum(10 +  16 * 4 + 16, 30 + obj->label * 16, voltage, 3, 16, 0x80, WHITE);   /* 显示小数部分 */

        vTaskDelay(100);
    }
}

/**
 * @brief       ap3216c测试
 * @param       无
 * @retval      无
 */
int ap3216c_test(Test_Typedef * obj)
{
    uint8_t key = 0;
    uint8_t xl_key = 0;
    uint16_t ir = 0, als = 0, ps = 0;
    
    printf("\r\n/******************************* 6，光环境传感器测试 **********************************/\r\n");
    printf("/*** 注意：可以遮挡AP3216C，并观察环境光强度（ALS）、接近距离（PS）和红外线强度（IR）等参数 ***/\r\n");
    printf("/****************************** NEXT: BOOT ;Record: KEY0 *******************************/\r\n");
    lcd_show_string(10 + 16 + 16 * 3, 30 + obj->label * 16 + 16 * 2, lcd_self.width, 16, 16, ":IR", RED);
    lcd_show_string(10 + 16 + 16 * 3, 30 + obj->label * 16 + 16 * 3, lcd_self.width, 16, 16, ":PS", RED);
    lcd_show_string(10 + 16 + 16 * 3, 30 + obj->label * 16 + 16 * 4, lcd_self.width, 16, 16, ":ALS", RED);
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);

    while(1)
    {
        key = key_scan(0);
        xl_key = xl9555_key_scan(0);

        if (key == BOOT_PRES)
        {
            lcd_fill(10 + 16,30 + obj->label * 16 + 16 * 2,10 + 16 + 16 * 6,30 + obj->label * 16 + 16 * 4 + 16,BLACK);
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
            printf("******OK!******\r\n");
            return TEST_OK;
        }

        if (xl_key == KEY0_PRES)
        {
            lcd_fill(10 + 16,30 + obj->label * 16 + 16 * 2,10 + 16 + 16 * 6,30 + obj->label * 16 + 16 * 4 + 16,BLACK);
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
            printf("******FAIL!******\r\n");
            return TEST_FAIL;
        }

        ap3216c_read_data(&ir, &ps, &als);                                          /* 读取数据  */
        printf("IR:%d,PS:%d,ALS:%d\r\n",ir, ps, als);
        lcd_show_num(10 + 16, 30 + obj->label * 16 + 16 * 2, ir, 5, 16, WHITE);      /* 显示IR数据 */
        lcd_show_num(10 + 16, 30 + obj->label * 16 + 16 * 3, ps, 5, 16, WHITE);      /* 显示PS数据 */
        lcd_show_num(10 + 16, 30 + obj->label * 16 + 16 * 4, als, 5, 16, WHITE);     /* 显示ALS数据  */

        vTaskDelay(100);
    }
}

/**
 * @brief       sd测试
 * @param       无
 * @retval      无
 */
int sd_test(Test_Typedef * obj)
{
    uint8_t key = 0;
    uint8_t xl_key = 0;
    
    printf("\r\n/******************************* 7，SD卡测试 **********************************/\r\n");
    printf("/*** 注意：请插入SD卡到TF卡槽当中,按下BOOT按键标志检测失败。 ***/\r\n");
    printf("/*************************************** NEXT: AUTO **************************************/\r\n\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);

    while(sd_spi_init())
    {
        key = key_scan(0);
        xl_key = xl9555_key_scan(0);

        lcd_show_string(10 + 16 * 4, 30 + obj->label * 16, lcd_self.width, 16, 16, "SDError", RED);

        if (xl_key == KEY0_PRES || key == BOOT_PRES)
        {
            lcd_fill(10 + 16 * 4,30 + obj->label * 16,10 + 16 + 16 * 7,30 + obj->label * 16 + 16 * 2,BLACK);
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
            printf("******FAIL!******\r\n");
            return TEST_FAIL;
        }

        vTaskDelay(100);
    }

    printf("******OK!******\r\n");
    lcd_fill(10 + 16 * 4,30 + obj->label * 16,10 + 16 + 16 * 7,30 + obj->label * 16 + 16 * 2,BLACK);
    lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
    return TEST_OK;
}

/**
 * @brief       qma6100p测试
 * @param       无
 * @retval      无
 */
int qma6100p_test(Test_Typedef * obj)
{
    static uint8_t id_data[2];
    
    printf("\r\n/******************************* 8，QMA6100P测试测试 **********************************/\r\n");
    printf("/********************************** NEXT: AUTO ********************************/\r\n\r\n");
    qma6100p_register_read(QMA6100P_REG_CHIP_ID, id_data, 1);    /* 读取设备ID，正常是0x90 */
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);

    if (id_data[0] == 0x90)
    {
        printf("******OK!******\r\n");
        lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
        return TEST_OK;
    }
    else
    {
        printf("******FAIL!******\r\n");
        lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
        return TEST_FAIL;
    }
}

uint8_t rmt_flag;

/**
 * @brief       rmt测试
 * @param       obj:句柄
 * @retval      无
 */
int rmt_test(Test_Typedef * obj)
{
    uint8_t key = 0;
    rmt_flag = 0;
    printf("\r\n/******************************* 9，红外收发 *******************************/\r\n");
    printf("/* 注意：请接上P3处的跳线帽，将AIN与RMT进行短接，然后观看红外接收的数据是否是0~255 */\r\n");
    printf("/****************************** NEXT: BOOT *******************************/\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);
    lcd_show_string(10 + 16 * 4, 30 + obj->label * 16, lcd_self.width, 16, 16,"P3:AIN>RMT?",RED);      /* 显示电压值的整数部分 */

    while(1)
    {
        key = key_scan(0);

        if (key == BOOT_PRES)
        {
            break;
        }

        vTaskDelay(100);
    }
    printf("******确认P3端的AIN和RMT已接上******\r\n");
    lcd_fill(10 + 16 * 4,30 + obj->label * 16,30 + obj->label * 16 + 16 * 4,30 + obj->label * 16 + 16,BLACK);
    printf("/****************************** NEXT: AUTO ;Record: 长按KEY0 *******************************/\r\n");
    emission_init();

    if (rmt_flag == 1)
    {
        lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
    }
    else
    {
        lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
    }

    return TEST_OK;
}

extern const uint8_t music_pcm_start[] asm("_binary_canon_pcm_start");
extern const uint8_t music_pcm_end[]   asm("_binary_canon_pcm_end");

/**
 * @brief       es8388_test测试
 * @param       无
 * @retval      无
 */
int es8388_test(Test_Typedef * obj)
{
    size_t bytes_write = 0;
    
    printf("\r\n/******************************* 10，MP3测试 *******************************/\r\n");
    printf("/* 注意：需插入TF卡，并且TF卡需创建MUSIC文件夹，将test.wav音乐文件拷贝进去 */\r\n");
    printf("/********************************** NEXT: AUTO ********************************/\r\n\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);
    i2s_init();                                         /* I2S初始化 */
    es8388_adda_cfg(1, 0);                              /* 开启DAC关闭ADC */
    es8388_input_cfg(0);                                /* 关闭输入 */
    es8388_output_cfg(1, 1);                            /* DAC选择通道输出 */
    es8388_hpvol_set(30);                               /* 设置耳机音量 */
    es8388_spkvol_set(30);                              /* 设置喇叭音量 */
    xl9555_pin_write(SPK_EN_IO,0);                      /* 打开喇叭 */

    while (1)
    {
        bytes_write = i2s_tx_write(music_pcm_start, music_pcm_end - music_pcm_start);

        if (bytes_write > 0)
        {
            printf("******OK!******\r\n");
            i2s_deinit();                               /* 卸载I2S */
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
            return TEST_OK;
        }
        else
        {
            printf("******FAIL!******\r\n");
            i2s_deinit();                               /* 卸载I2S */
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
            return TEST_FAIL;
        }

        vTaskDelay(10);
    }

    return TEST_OK;
}


static uint8_t usb_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
__usbdev g_usbdev;                              /* USB控制器 */

/**
 * @brief       SD卡模拟U盘函数初始化
 * @param       itf     :设置的简要CDC端口
 * @param       event   :CDC事件
 * @retval      无
 */
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    size_t rx_size = 0;

    /* 读取串口的数据 */
    tinyusb_cdcacm_read(itf, usb_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);

    /* 发送数据 */
    tinyusb_cdcacm_write_queue(itf, usb_buf, rx_size);
    tinyusb_cdcacm_write_flush(itf, 0);
}

/**
 * @brief       SD卡模拟U盘函数初始化
 * @param       itf     :设置的简要CDC端口
 * @param       event   :CDC事件
 * @retval      无
 */
void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    g_usbdev.status |= 0x01;
}

extern esp_err_t tinyusb_cdc_deinit(int itf);

/**
 * @brief       卸载USART
 * @param       无
 * @retval      无
 */
void tud_usb_detint(void)
{
    tusb_stop_task();
    tinyusb_cdc_deinit(TINYUSB_CDC_ACM_0);
}

/**
 * @brief      USB函数初始化
 * @param       无
 * @retval      无
 */
void tud_usb_usart(void)
{
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };
    /* USB设备登记 */
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,                /* USB设备 */
        .cdc_port = TINYUSB_CDC_ACM_0,              /* CDC端口 */
        .rx_unread_buf_sz = 64,                     /* 配置RX缓冲区大小 */
        .callback_rx = &tinyusb_cdc_rx_callback,    /* 接收回调函数 */
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    /* USB CDC初始化 */
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    /* 注册回调函数 */
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
                        TINYUSB_CDC_ACM_0,
                        CDC_EVENT_LINE_STATE_CHANGED,
                        &tinyusb_cdc_line_state_changed_callback));
}

/**
 * @brief       usb_test测试
 * @param       无
 * @retval      无
 */
int usb_test(Test_Typedef * obj)
{
    uint8_t xl_key = 0;

    printf("/******************************* 11，USB测试 *******************************/\r\n");
    printf("/* 注意：请将USB线连接到USB_SLAVE接口上，电脑检测到USB端口自动退出 */\r\n");
    printf("/********************************** NEXT: AUTO ********************************/\r\n\r\n");
    lcd_show_string(10, 30 + obj->label * 16, lcd_self.width, 16, 16, obj->name_test, WHITE);
    tud_usb_usart();                                    /* USB初始化 */

    while(1)
    {
        xl_key = xl9555_key_scan(0);

        if ((g_usbdev.status & 0x0f) == 0x01)
        {
            printf("******OK!******\r\n");
            tud_usb_detint();
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "OK", WHITE);
            return TEST_OK;
        }

        if (xl_key == KEY0_PRES)
        {
            printf("******FAIL!******\r\n");
            lcd_show_string(lcd_self.width - 95, 30 + obj->label * 16, lcd_self.width, 16, 16, "FAIL", RED);
            return TEST_FAIL;
        }

        vTaskDelay(10);
    }
}

/* 正点原子logo 图标(24*24大小)
   PCtoLCD2002取模方式:阴码,逐行式,顺向 */
const uint8_t APP_ALIENTEK_ICO2424[]=
{
    0x00,0xFF,0x80,0x03,0xFF,0xE0,0x06,0x00,0xF0,0x18,0x70,0x38,0x07,0xFE,0x1C,0x1C,
    0x0F,0x86,0x30,0x01,0xCE,0x4F,0xE0,0x7F,0x3F,0xF8,0x3F,0xFF,0xF0,0x7F,0xFF,0xE0,
    0xFF,0xFF,0xC1,0xFF,0xFF,0x83,0xFF,0xFE,0x07,0xFF,0x7E,0x0F,0xFE,0x7F,0x07,0xF1,
    0x3B,0xC0,0x06,0x30,0xF0,0x3C,0x18,0x3F,0xF0,0x0E,0x07,0x0C,0x0F,0x80,0x30,0x03,
    0xFF,0xE0,0x00,0xFF,0x80,0x00,0x3C,0x00,
};

/**
 * @brief       界面
 * @param       无
 * @retval      无
 */
void app_ui_disp(void)
{
    lcd_clear(BLACK);
    lcd_app_show_mono_icos(5,2,24,24,(uint8_t*)APP_ALIENTEK_ICO2424,YELLOW,BLACK);
    lcd_show_string(24 + 10, 2, lcd_self.width, 24, 24, "ALIENTEK ESP32-S3", WHITE);
    lcd_draw_rectangle(5,25,lcd_self.width - 25,lcd_self.height - 25,WHITE);
    lcd_show_string(5, lcd_self.height - 20, lcd_self.width, 16, 16, "KEY0:Record", WHITE);
    lcd_show_string(lcd_self.width - 95 , lcd_self.height - 20, lcd_self.width, 16, 16, "BOOT:Next", WHITE);
}

const uint8_t g_clear_buf[] = {"000000"};

/**
 * @brief       板载功能测试
 * @param       无
 * @retval      无
 */
void func_test(void)
{
    memset(datatemp,0,TEXT_SIZE);

    if (key_scan(0) == BOOT_PRES)
    {
        /* 写数据 */
        at24cxx_write(0, (uint8_t *)g_clear_buf, TEXT_SIZE);
        vTaskDelay(100);
    }

    /* 读数据 */
    at24cxx_read(0, datatemp, TEXT_SIZE);
    vTaskDelay(100);

    /* 匹配写和读数据是否一致 */
    if (strcmp((char *)datatemp, (char *)g_text_buf) != 0)
    {
        lcd_display_dir(1);                 /* 设置横屏 */
        app_ui_disp();                      /* 界面显示 */

        printf("\r\n**************************************\r\n");
        printf("\r\n***ESP32-S3 Full Functional Testing***\r\n");
        printf("\r\n******KEY0:Next******BOOT:Record******\r\n");
        printf("\r\n**************************************\r\n");

        test_create("LED",led_test);
        test_create("KEY",key_test);
        test_create("BEEP",beep_test);
        test_create("AT24",at24cx_test);
        test_create("ADC",adc_test);
        test_create("AP3216C",ap3216c_test);
        test_create("SD",sd_test);
        test_create("QMA6100P",qma6100p_test);
        test_create("RMT",rmt_test);
        test_create("ES8388",es8388_test);
        test_create("USB",usb_test);
        test_handler();
        printf("\r\n/******************************* 测试完成 *******************************/\r\n");
        lcd_clear(WHITE);               /* 清屏 */
    }

    lcd_display_dir(0);                 /* 设置竖屏 */
}
