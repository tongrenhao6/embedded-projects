/*
  电子相册 
  中间显示图片，左右两个按钮切上一张/下一张
 */

#include "photo_album.h"
#include "lvgl/lvgl.h"
#include <string.h>

/* 一共几张图 */
#define IMG_COUNT 5

/* 图片文件名，F: 对应开发板的 /root/gyb/ 目录 */
static const char * img_files[IMG_COUNT] = {
    "F:1.png", "F:2.png", "F:3.png", "F:Doro800.png",
    "F:5.png"
};

static int cur_idx;              // 当前第几张
static lv_obj_t * img;           // 图片控件
static lv_obj_t * page_label;    // 显示 "1/5" 的文字
static lv_obj_t * list1;

// 显示第 idx 张图片
static void show_image(int idx)
{
    lv_img_set_src(img, img_files[idx]);  // 加载图片

    // 更新页码
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%d/%d", idx + 1, IMG_COUNT);
    lv_label_set_text(page_label, buf);
}

// // 上一张
// static void prev_cb(lv_event_t * e)
// {
//     cur_idx--;
//     if (cur_idx < 0) cur_idx = IMG_COUNT - 1;  // 到头就回到最后一张
//     show_image(cur_idx);
// }

// // 下一张
// static void next_cb(lv_event_t * e)
// {
//     cur_idx++;
//     if (cur_idx >= IMG_COUNT) cur_idx = 0;  // 到底就回到第一张
//     show_image(cur_idx);
// }

// 按钮响应事件
static void button_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    if(strcmp(lv_list_get_btn_text(list1, obj),"prev") == 0)
    {
        cur_idx--;
        if (cur_idx < 0) cur_idx = IMG_COUNT - 1;  // 到头就回到最后一张
        show_image(cur_idx);
    }
    else if(strcmp(lv_list_get_btn_text(list1, obj),"next") == 0)
    {
        cur_idx++;
        if (cur_idx >= IMG_COUNT) cur_idx = 0;  // 到底就回到第一张
        show_image(cur_idx);
    }
}


void photo_album_create(void)
{
    cur_idx = 0;

    // 清空屏幕，背景黑色
    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // ---- 图片（居中） ----
    img = lv_img_create(scr);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    // ---- 上一张按钮（左边） ----
    lv_obj_t * prev = lv_btn_create(scr);
    lv_obj_set_size(prev, 70, 100);
    lv_obj_align(prev, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(prev, lv_color_make(0,0,255), 0);
    lv_obj_add_event_cb(prev, button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl1 = lv_label_create(prev);
    lv_label_set_text(lbl1, "prev");
    lv_obj_center(lbl1);

    // ---- 下一张按钮（右边） ----
    lv_obj_t * next = lv_btn_create(scr);
    lv_obj_set_size(next, 70, 100);
    lv_obj_align(next, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(next, lv_color_make(0,0,255), 0);
    
    lv_obj_add_event_cb(next, button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl2 = lv_label_create(next);
    lv_label_set_text(lbl2, "next");
    lv_obj_center(lbl2);

    // ---- 页码（底部居中） ----
    page_label = lv_label_create(scr);
    lv_obj_set_style_text_color(page_label, lv_color_make(150, 150, 150), 0);
    lv_obj_align(page_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // 显示第一张
    show_image(0);
}
