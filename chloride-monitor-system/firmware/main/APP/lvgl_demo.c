/**
 ****************************************************************************************************
 * @file        lvgl_demo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-12-01
 * @brief       LVGL V8移植 实验
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

#include "lvgl_demo.h"
#include "demos/lv_demos.h"
#include "app_ui.h"
#include "image.h"
#include "sdmmc_cmd.h"
#include "ph_sampler.h"

static const char *TAG = "lvgl_demo";


lv_indev_t *indev_keypad;       /* 按键组 */
uint32_t back_act_key = 0;      /* 返回主界面按键 */
uint8_t sd_check_en = 0;        /* sd卡检测标志 */
uint8_t decode_en = ESP_OK;     /* 图片解码后才能退出 */
SemaphoreHandle_t xGuiSemaphore;

/* 函数声明 */
static void lvgl_disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void keypad_init(void);
static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static uint32_t keypad_get_key(void);
void lv_port_indev_init(void);
void increase_lvgl_tick(void *arg);
void lv_port_disp_init(void);


/* WATCH 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define WATCH_PRIO      1                   /* 任务优先级 */
#define WATCH_STK_SIZE  5*1024              /* 任务堆栈大小 */
TaskHandle_t            WATCHTask_Handler;  /* 任务句柄 */
void watch(void *pvParameters);             /* 任务函数 */

SemaphoreHandle_t BinarySemaphore;          /* 二值信号量句柄 */

/**
 * @brief       watch
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void watch(void *pvParameters)
{
    pvParameters = pvParameters;

    while(1)
    {
        LED_TOGGLE();

        vTaskDelay(500);
    }
}

/**
 * @brief       lvgl_demo入口函数
 * @param       无
 * @retval      无
 */
void lvgl_demo(void)
{
    ESP_LOGI(TAG, "enter lvgl_demo");
    while(images_init())    /* 初始化图片库 */
    {
        while (sd_spi_init())   /* 检测不到SD卡 */
        {
            lcd_show_string(30, 50, 200, 16, 16, "SD Card Failed!", RED);
            ESP_LOGW(TAG, "sd_spi_init failed, waiting for sd card");
            vTaskDelay(1000);
        }

        sd_check_en = 1;        /* 检测SD卡 */
        
        lcd_show_string(30, 50, 200, 16, 16, "image Error!", RED);
        images_update_image(30, 90, 16, (uint8_t *)"0:", RED);
        vTaskDelay(1000);
        lcd_clear(WHITE);
    }

    if (xl9555_key_scan(0) == KEY3_PRES)
    {
        images_update_image(30, 90, 16, (uint8_t *)"0:", RED);
    }
    
    /* 创建二值信号量 */
    BinarySemaphore = xSemaphoreCreateBinary();
    xGuiSemaphore = xSemaphoreCreateMutex();

    if (WATCHTask_Handler == NULL)
    {
        /* 创建WATCH任务 */
        xTaskCreatePinnedToCore((TaskFunction_t )watch,                 /* 任务函数 */
                                (const char*    )"watch",               /* 任务名称 */
                                (uint16_t       )WATCH_STK_SIZE,        /* 任务堆栈大小 */
                                (void*          )NULL,                  /* 传入给任务函数的参数 */
                                (UBaseType_t    )WATCH_PRIO,            /* 任务优先级 */
                                (TaskHandle_t*  )&WATCHTask_Handler,    /* 任务句柄 */
                                (BaseType_t     ) 1);                   /* 该任务哪个内核运行 */
    }

    lv_init();              /* 初始化LVGL图形库 */
    lv_port_disp_init();    /* lvgl显示接口初始化,放在lv_init()的后面 */
    lv_port_indev_init();   /* lvgl输入接口初始化,放在lv_init()的后面 */

    /* 为LVGL提供时基单元 */
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 1 * 1000));

    lv_load_main_window();
    ph_sampler_start();     /* Start after SD/image resources are initialized. */

    while (1)
    {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }

        vTaskDelay(pdMS_TO_TICKS(5));  /* 延时5毫秒 */
    }
}

/**
 * @brief       初始化并注册显示设备
 * @param       无
 * @retval      无
 */
void lv_port_disp_init(void)
{
    void *buf1 = NULL;
    void *buf2 = NULL;

    /* 初始化显示设备LCD */

    /*-----------------------------
     * 创建一个绘图缓冲区
     *----------------------------*/
    /**
     * LVGL 需要一个缓冲区用来绘制小部件
     * 随后，这个缓冲区的内容会通过显示设备的 'flush_cb'(显示设备刷新函数) 复制到显示设备上
     * 这个缓冲区的大小需要大于显示设备一行的大小
     *
     * 这里有3种缓冲配置:
     * 1. 单缓冲区:
     *      LVGL 会将显示设备的内容绘制到这里，并将他写入显示设备。
     *
     * 2. 双缓冲区:
     *      LVGL 会将显示设备的内容绘制到其中一个缓冲区，并将他写入显示设备。
     *      需要使用 DMA 将要显示在显示设备的内容写入缓冲区。
     *      当数据从第一个缓冲区发送时，它将使 LVGL 能够将屏幕的下一部分绘制到另一个缓冲区。
     *      这样使得渲染和刷新可以并行执行。
     *
     * 3. 全尺寸双缓冲区
     *      设置两个屏幕大小的全尺寸缓冲区，并且设置 disp_drv.full_refresh = 1。
     *      这样，LVGL将始终以 'flush_cb' 的形式提供整个渲染屏幕，您只需更改帧缓冲区的地址。
     */
    /* 创建一个绘图缓冲区 */
    buf1 = heap_caps_malloc(lcd_self.width * 10 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    /* 初始化显示缓冲区 */
    static lv_disp_draw_buf_t disp_buf;                                 /* 保存显示缓冲区信息的结构体 */
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, lcd_self.width * 10);  /* 初始化显示缓冲区 */
    
    /* 在LVGL中注册显示设备 */
    static lv_disp_drv_t disp_drv;      /* 显示设备的描述符(HAL要注册的显示驱动程序、与显示交互并处理与图形相关的结构体、回调函数) */
    lv_disp_drv_init(&disp_drv);        /* 初始化显示设备 */
    
    /* 设置显示设备的分辨率 
     * 这里为了适配正点原子的多款屏幕，采用了动态获取的方式，
     * 在实际项目中，通常所使用的屏幕大小是固定的，因此可以直接设置为屏幕的大小 
     */
    disp_drv.hor_res = lcd_self.width;
    disp_drv.ver_res = lcd_self.height;

    /* 用来将缓冲区的内容复制到显示设备 */
    disp_drv.flush_cb = lvgl_disp_flush_cb;

    /* 设置显示缓冲区 */
    disp_drv.draw_buf = &disp_buf;

    /* 注册显示设备 */
    lv_disp_drv_register(&disp_drv);
}

/**
* @brief    将内部缓冲区的内容刷新到显示屏上的特定区域
* @note     可以使用 DMA 或者任何硬件在后台加速执行这个操作
*           但是，需要在刷新完成后调用函数 'lv_disp_flush_ready()'
* @param    disp_drv : 显示设备
* @param    area : 要刷新的区域，包含了填充矩形的对角坐标
* @param    color_map : 颜色数组
* @retval   无
*/
static void lvgl_disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lcd_color_fill(area->x1,area->y1,area->x2,area->y2,(uint16_t*)color_map);
    /* 重要!!! 通知图形库，已经刷新完毕了 */
    lv_disp_flush_ready(drv);
}

/**
 * @brief       告诉LVGL运行时间
 * @param       arg : 传入参数(未用到)
 * @retval      无
 */
void increase_lvgl_tick(void *arg)
{
    /* 告诉LVGL已经过了多少毫秒 */
    lv_tick_inc(1);
}

/**
 * @brief       初始化并注册输入设备
 * @param       无
 * @retval      无
 */
void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    /* 初始化按键驱动 */
    keypad_init();

    /* 注册键盘输入设备 */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&indev_drv);
}

/**
 * @brief       初始化键盘
 * @param       无
 * @retval      无
 */
static void keypad_init(void)
{
    
}

uint32_t g_last_key = 0;

/**
 * @brief       图形库的键盘读取回调函数
 * @param       indev_drv : 键盘设备
 *   @arg       data      : 输入设备数据结构体
 * @retval      无
 */
static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    /* 获取按键是否被按下，并保存键值 */
    uint32_t act_key = keypad_get_key();

    if(act_key != 0) 
    {
        data->state = LV_INDEV_STATE_PR;

        /* 将键值转换成 LVGL 的控制字符 */
        switch(act_key) 
        {
            case KEY0_PRES:
                act_key = LV_KEY_NEXT;
            break;

            case KEY1_PRES:
                act_key = KEY1_PRES;
                back_act_key = KEY1_PRES;
            break;

            case KEY2_PRES:
                act_key = LV_KEY_PREV;
            break;
            
            case KEY3_PRES:
                act_key = LV_KEY_ENTER;
            break;
        }

        g_last_key = act_key;
    } 
    else 
    {
        data->state = LV_INDEV_STATE_REL;
        g_last_key = 0;
    }

    if (back_act_key == KEY1_PRES && decode_en == ESP_OK)
    {
        if (app_obj_general.current_parent != NULL || app_obj_general.del_parent != NULL)
        {
            app_obj_general.Function();
        }

        back_act_key = 0;
    }

    data->key = g_last_key;
}

/**
 * @brief       获取当前正在按下的按键
 * @param       无
 * @retval      0 : 按键没有被按下
 */
static uint32_t keypad_get_key(void)
{
    return xl9555_key_scan(0);
}
