/**
 ****************************************************************************************************
 * @file        lv_measure_demo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       测量系统
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 */

#include "lv_measure_demo.h"

#include <stdio.h>


LV_IMG_DECLARE(ui_img_img_slider_png);
LV_IMG_DECLARE(ui_img_pattern_png);
measure_obj_t measure_obj;
static ph_sensor_data_t ph_sensor_data;

static void set_float_label(lv_obj_t *label, float value, int decimals)
{
    char text[16];
    int scaled;
    int integer;
    int fraction;
    int divisor = 1;

    for (int i = 0; i < decimals; i++)
    {
        divisor *= 10;
    }

    scaled = (int)(value * divisor + (value >= 0.0f ? 0.5f : -0.5f));
    integer = scaled / divisor;
    fraction = scaled % divisor;

    if (fraction < 0)
    {
        fraction = -fraction;
    }

    if (decimals == 1)
    {
        snprintf(text, sizeof(text), "%d.%01d", integer, fraction);
    }
    else
    {
        snprintf(text, sizeof(text), "%d.%02d", integer, fraction);
    }

    lv_label_set_text(label, text);
}
/**
  * @brief  删除测量demo
  * @param  无
  * @retval 无
  */
void lv_measure_del(void)
{
    if (measure_obj.small_cont.temp.lv_temp_timer != NULL)
    {
        lv_timer_del(measure_obj.small_cont.temp.lv_temp_timer);
    }

    if (measure_obj.small_cont.ap3216c.lv_ap3216c_timer != NULL)
    {
        lv_timer_del(measure_obj.small_cont.ap3216c.lv_ap3216c_timer);
    }

    lv_obj_clean(app_obj_general.del_parent);
    lv_obj_del(app_obj_general.del_parent);
    app_obj_general.del_parent = NULL;
    lv_app_show();
}

/**
 * @brief  获取PH传感器信息
  * @param  无
  * @retval 无
  */
static void lv_ph_sensor_timer(lv_timer_t* timer)
{
    esp_err_t ret = ph_sensor_read(&ph_sensor_data);

    if (ret == ESP_OK)
    {
        set_float_label(measure_obj.small_cont.ap3216c.ap_ir, ph_sensor_data.ph, 2);
        set_float_label(measure_obj.small_cont.ap3216c.ap_ps, ph_sensor_data.mv, 1);
        set_float_label(measure_obj.small_cont.ap3216c.ap_als, ph_sensor_data.temperature, 1);
        lv_label_set_text_fmt(measure_obj.small_cont.temp.temp_obj, "A%u:%lu",
                              ph_sensor_data.address,
                              (unsigned long)ph_sensor_data.ok_count);
        lv_slider_set_value(measure_obj.small_cont.temp.temp_slider, (int)ph_sensor_data.ph, LV_ANIM_ON);
    }
    else
    {
        lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ir, "--");
        lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ps, "--");
        lv_label_set_text(measure_obj.small_cont.ap3216c.ap_als, "--");
        lv_label_set_text_fmt(measure_obj.small_cont.temp.temp_obj, "%s:%lu",
                              ph_sensor_get_last_error(),
                              (unsigned long)ph_sensor_data.err_count);
    }
}

/**
 * @brief       测量demo
 * @param       无
 * @retval      无
 */
void lv_measure_demo(void)
{
    /* 解决重复按下 */
    if (app_obj_general.current_parent != NULL)
    {
        lv_obj_del(app_obj_general.current_parent);
        app_obj_general.current_parent = NULL;
    }
    
    ph_sensor_init();                                           /* 初始化RS485 PH传感器 */

    lv_app_del();

    measure_obj.lv_main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_style_radius(measure_obj.lv_main_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(measure_obj.lv_main_cont, lv_color_make(0,0,0), LV_STATE_DEFAULT);
    lv_obj_set_size(measure_obj.lv_main_cont,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()));
    lv_obj_set_style_border_opa(measure_obj.lv_main_cont,LV_OPA_0,LV_STATE_DEFAULT);
    lv_obj_set_pos(measure_obj.lv_main_cont,0,20);

    app_obj_general.del_parent = measure_obj.lv_main_cont;              /* 指向当前界面容器 */
    app_obj_general.Function = lv_measure_del;                              /* 删除此界面 */

    measure_obj.small_cont.temp.temp_slider = lv_slider_create(measure_obj.lv_main_cont);
    lv_slider_set_value(measure_obj.small_cont.temp.temp_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(measure_obj.small_cont.temp.temp_slider,0,14);
    if(lv_slider_get_mode(measure_obj.small_cont.temp.temp_slider) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(measure_obj.small_cont.temp.temp_slider, 0, LV_ANIM_OFF);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_slider, 19);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_slider, 235);
    lv_obj_align(measure_obj.small_cont.temp.temp_slider,LV_ALIGN_LEFT_MID,15,0);
    lv_obj_set_style_radius(measure_obj.small_cont.temp.temp_slider, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(measure_obj.small_cont.temp.temp_slider, lv_color_hex(0xD0D7DF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(measure_obj.small_cont.temp.temp_slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(measure_obj.small_cont.temp.temp_slider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(measure_obj.small_cont.temp.temp_slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(measure_obj.small_cont.temp.temp_slider, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(measure_obj.small_cont.temp.temp_slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(measure_obj.small_cont.temp.temp_slider, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(measure_obj.small_cont.temp.temp_slider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(measure_obj.small_cont.temp.temp_slider, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(measure_obj.small_cont.temp.temp_slider, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(measure_obj.small_cont.temp.temp_slider, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(measure_obj.small_cont.temp.temp_slider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(measure_obj.small_cont.temp.temp_slider, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(measure_obj.small_cont.temp.temp_slider, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(measure_obj.small_cont.temp.temp_slider, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(measure_obj.small_cont.temp.temp_slider, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(measure_obj.small_cont.temp.temp_slider, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(measure_obj.small_cont.temp.temp_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(measure_obj.small_cont.temp.temp_slider, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(measure_obj.small_cont.temp.temp_slider, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(measure_obj.small_cont.temp.temp_slider, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(measure_obj.small_cont.temp.temp_slider, &ui_img_img_slider_png, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(measure_obj.small_cont.temp.temp_slider, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    measure_obj.small_cont.temp.temp_line1 = lv_obj_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_line1, 10);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_line1, 2);
    lv_obj_set_x(measure_obj.small_cont.temp.temp_line1, 0);
    lv_obj_set_y(measure_obj.small_cont.temp.temp_line1, 110);
    lv_obj_set_style_bg_color(measure_obj.small_cont.temp.temp_line1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(measure_obj.small_cont.temp.temp_line1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(measure_obj.small_cont.temp.temp_line1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(measure_obj.small_cont.temp.temp_line1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(measure_obj.small_cont.temp.temp_line1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.temp.temp_line2 = lv_obj_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_line2, 10);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_line2, 2);
    lv_obj_set_x(measure_obj.small_cont.temp.temp_line2, 0);
    lv_obj_set_y(measure_obj.small_cont.temp.temp_line2, 190);
    lv_obj_set_style_bg_color(measure_obj.small_cont.temp.temp_line2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(measure_obj.small_cont.temp.temp_line2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(measure_obj.small_cont.temp.temp_line2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(measure_obj.small_cont.temp.temp_line2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(measure_obj.small_cont.temp.temp_line2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.temp.temp_line2_1 = lv_obj_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_line2_1, 10);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_line2_1, 2);
    lv_obj_set_x(measure_obj.small_cont.temp.temp_line2_1, 37);
    lv_obj_set_y(measure_obj.small_cont.temp.temp_line2_1, 190);
    lv_obj_set_style_bg_color(measure_obj.small_cont.temp.temp_line2_1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(measure_obj.small_cont.temp.temp_line2_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(measure_obj.small_cont.temp.temp_line2_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(measure_obj.small_cont.temp.temp_line2_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(measure_obj.small_cont.temp.temp_line2_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.temp.temp_line1_1 = lv_obj_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_line1_1, 10);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_line1_1, 2);
    lv_obj_set_x(measure_obj.small_cont.temp.temp_line1_1, 37);
    lv_obj_set_y(measure_obj.small_cont.temp.temp_line1_1, 110);
    lv_obj_set_style_bg_color(measure_obj.small_cont.temp.temp_line1_1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(measure_obj.small_cont.temp.temp_line1_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(measure_obj.small_cont.temp.temp_line1_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(measure_obj.small_cont.temp.temp_line1_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(measure_obj.small_cont.temp.temp_line1_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_ir = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_ir, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_ir, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_ir, -10);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_ir, 10);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_ir, LV_ALIGN_TOP_MID);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ir, " ");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_ir, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_ir, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_ir, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_ps = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_ps, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_ps, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_ps, -10);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_ps, 80);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_ps, LV_ALIGN_TOP_MID);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ps, " ");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_ps, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_ps, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_ps, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_als = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_als, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_als, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_als, -10);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_als, 150);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_als, LV_ALIGN_TOP_MID);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_als, " ");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_als, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_als, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_als, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.temp.temp_obj = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_obj, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_obj, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.temp.temp_obj, -10);
    lv_obj_set_y(measure_obj.small_cont.temp.temp_obj, 230);
    lv_obj_set_align(measure_obj.small_cont.temp.temp_obj, LV_ALIGN_TOP_MID);
    lv_label_set_text(measure_obj.small_cont.temp.temp_obj, " ");
    lv_obj_set_style_text_color(measure_obj.small_cont.temp.temp_obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.temp.temp_obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.temp.temp_obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_ir_label = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_ir_label, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_ir_label, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_ir_label, -15);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_ir_label, 20);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_ir_label, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ir_label, "PH");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_ir_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_ir_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_ir_label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_ir_unit = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_ir_unit, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_ir_unit, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_ir_unit, -15);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_ir_unit, 39);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_ir_unit, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ir_unit, "pH");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_ir_unit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_ir_unit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_ir_unit, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_ps_label = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_ps_label, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_ps_label, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_ps_label, -15);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_ps_label, 90);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_ps_label, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ps_label, "MV");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_ps_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_ps_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_ps_label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_ps_unit = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_ps_unit, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_ps_unit, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_ps_unit, -15);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_ps_unit, 109);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_ps_unit, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_ps_unit, "mV");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_ps_unit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_ps_unit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_ps_unit, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_als_label = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_als_label, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_als_label, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_als_label, -15);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_als_label, 163);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_als_label, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_als_label, "TEMP");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_als_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_als_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_als_label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.ap3216c.ap_als_unit = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.ap3216c.ap_als_unit, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.ap3216c.ap_als_unit, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.ap3216c.ap_als_unit, -15);
    lv_obj_set_y(measure_obj.small_cont.ap3216c.ap_als_unit, 182);
    lv_obj_set_align(measure_obj.small_cont.ap3216c.ap_als_unit, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.ap3216c.ap_als_unit, "C");
    lv_obj_set_style_text_color(measure_obj.small_cont.ap3216c.ap_als_unit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.ap3216c.ap_als_unit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.ap3216c.ap_als_unit, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.temp.temp_label = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_label, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_label, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.temp.temp_label, -15);
    lv_obj_set_y(measure_obj.small_cont.temp.temp_label, 236);
    lv_obj_set_align(measure_obj.small_cont.temp.temp_label, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.temp.temp_label, "STAT");
    lv_obj_set_style_text_color(measure_obj.small_cont.temp.temp_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.temp.temp_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.temp.temp_label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    measure_obj.small_cont.temp.temp_unit = lv_label_create(measure_obj.lv_main_cont);
    lv_obj_set_width(measure_obj.small_cont.temp.temp_unit, LV_SIZE_CONTENT);
    lv_obj_set_height(measure_obj.small_cont.temp.temp_unit, LV_SIZE_CONTENT);
    lv_obj_set_x(measure_obj.small_cont.temp.temp_unit, -15);
    lv_obj_set_y(measure_obj.small_cont.temp.temp_unit, 255);
    lv_obj_set_align(measure_obj.small_cont.temp.temp_unit, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(measure_obj.small_cont.temp.temp_unit, "RS485");
    lv_obj_set_style_text_color(measure_obj.small_cont.temp.temp_unit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(measure_obj.small_cont.temp.temp_unit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(measure_obj.small_cont.temp.temp_unit, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* RS485 PH传感器 */
    measure_obj.small_cont.temp.lv_temp_timer =  lv_timer_create(lv_ph_sensor_timer, 1000, NULL);
    measure_obj.small_cont.ap3216c.lv_ap3216c_timer = NULL;

    lv_group_add_obj(ctrl_g, measure_obj.small_cont.temp.temp_unit);
    lv_group_focus_obj(measure_obj.small_cont.temp.temp_unit);                /* 聚焦当前APP */
}
