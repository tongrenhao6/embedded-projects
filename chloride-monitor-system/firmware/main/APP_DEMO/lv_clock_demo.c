/**
 ****************************************************************************************************
 * @file        lv_clock_demo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       时钟
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

#include "lv_clock_demo.h"


LV_IMG_DECLARE(watch_bg);
LV_IMG_DECLARE(hour);
LV_IMG_DECLARE(minute);
LV_IMG_DECLARE(second);

lv_obj_t *ui_Min;
lv_obj_t *ui_Hour;
lv_obj_t *ui_Sec;
lv_timer_t * lv_clock;
uint8_t Minute = 0;
uint8_t Hour = 0;
uint8_t Second = 0 ;

/**
  * @brief  时钟定时
  * @param  timer:句柄
  * @retval 无
  */
static void lv_meter_timer(lv_timer_t* timer)
{
    uint16_t h = app_obj_btn.rtc.hour * 300 + app_obj_btn.rtc.minute / 12 % 12 * 60;
    lv_img_set_angle(ui_Hour, h);
    lv_img_set_angle(ui_Min, app_obj_btn.rtc.minute * 6 * 10);
    lv_img_set_angle(ui_Sec, app_obj_btn.rtc.second * 6 * 10);
}


/**
  * @brief  删除时钟demo
  * @param  无
  * @retval 无
  */
void lv_clock_del(void)
{

    if (lv_clock != NULL)
    {
        lv_timer_del(lv_clock);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));  /* 延时5毫秒 */
        lv_clock = NULL;
    }

    lv_obj_clean(app_obj_general.del_parent);
    lv_obj_del(app_obj_general.del_parent);
    app_obj_general.del_parent = NULL;
    lv_app_show();
}

/**
 * @brief       时钟demo
 * @param       无
 * @retval      无
 */
void lv_clock_demo(void)
{
    lv_app_del();
    Minute = app_obj_btn.rtc.minute;
    Hour = app_obj_btn.rtc.hour;
    Second = app_obj_btn.rtc.second ;
    lv_obj_t* ui_Clock = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(ui_Clock,0,0);
    lv_obj_set_style_radius(ui_Clock, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_Clock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Clock, lv_color_make(0,0,0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Clock,LV_OPA_0,LV_STATE_DEFAULT);
    lv_obj_set_size(ui_Clock,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()));
    
    lv_obj_t * img = lv_img_create(ui_Clock);
    lv_img_set_src(img, &watch_bg);                     //加载表盘
    lv_obj_set_size(img, 200, 200);
    lv_obj_align_to(img, ui_Clock, LV_ALIGN_CENTER, 0, 0);

    ui_Hour = lv_img_create(ui_Clock);     // 时针
    lv_img_set_src( ui_Hour, &hour);
    lv_obj_align_to(ui_Hour, img,LV_ALIGN_CENTER, 0, 0);

    ui_Min = lv_img_create(ui_Clock);   // 分针
    lv_img_set_src( ui_Min, &minute);
    lv_obj_align_to(ui_Min, img,LV_ALIGN_CENTER, 0, 0);

    ui_Sec = lv_img_create(ui_Clock);   //秒针
    lv_img_set_src(ui_Sec, &second);
    lv_obj_align_to(ui_Sec, img,LV_ALIGN_CENTER, 0, 0);

    uint16_t h = app_obj_btn.rtc.hour * 300 + app_obj_btn.rtc.minute / 12 % 12 * 60;
    lv_img_set_angle(ui_Hour, h);
    lv_img_set_angle(ui_Min, app_obj_btn.rtc.minute * 6 * 10);
    lv_img_set_angle(ui_Sec, app_obj_btn.rtc.second * 6 * 10);

    app_obj_general.del_parent = ui_Clock;              /* 指向当前界面容器 */
    app_obj_general.Function = lv_clock_del;            /* 删除此界面 */

    if (lv_clock == NULL)
    {
        lv_clock = lv_timer_create(lv_meter_timer, 10, NULL);
    }
    
    lv_group_add_obj(ctrl_g, ui_Clock);
    lv_group_focus_obj(ui_Clock);                        /* 聚焦当前APP */
}