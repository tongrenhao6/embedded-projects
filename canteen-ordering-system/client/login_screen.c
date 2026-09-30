/*
 * 登录界面 - login_screen.c
 *
 * 使用 100ask 拼音输入法，支持中文输入
 * 布局：上方是登录表单，底部是拼音键盘（需要时才弹出）
 * 
 * 当登录成功，创建新屏幕，加载电子相册功能
 */

#include "login_screen.h"
#include "lvgl/lvgl.h"
#include "lv_lib_100ask-release-v8.x/lv_lib_100ask.h"
#include <string.h>
#include "photo_album.h"

/* 引入中文字体 */
LV_FONT_DECLARE(lv_font_source_han_sans_normal_16);

static lv_obj_t * kb;           // 拼音键盘
static lv_obj_t * ta_account;   // 账号输入框
static lv_obj_t * ta_password;  // 密码输入框
static lv_obj_t * status_label; // 状态提示（在登录按钮下方）

// 点击输入框时弹出键盘，按完成/取消时收起
static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        // 输入框获得焦点 → 弹出键盘，绑定到当前输入框
        if (lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // 点击了"完成"或"取消" → 收起键盘
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
        lv_indev_reset(NULL, ta);
    }
}

// 点击登录按钮
static void login_btn_cb(lv_event_t * e)
{   // 获取账户和密码输入框的文本
    const char * account = lv_textarea_get_text(ta_account);
    const char * password = lv_textarea_get_text(ta_password);

    // 检查是否为空
    if (account[0] == '\0' || password[0] == '\0') {
        lv_label_set_text(status_label, "账号或密码不能为空");
        return;
    }

    // 这里写你自己的验证逻辑，比如比对预设的账号密码
    if (strcmp(account, "admin") == 0 && strcmp(password, "123456") == 0) {
        lv_label_set_text(status_label, "登录成功！");
        // 登录成功就跳转到新屏幕

        // 为了节省一些内存，在加载电子相册后，把登录界面直接嘎掉，不考虑返回，力竭了
        // 记录旧屏幕
        lv_obj_t * old_scr = lv_scr_act();
        // 还要记录新屏幕 ，NULL表示新屏幕
        // lv_scr_load(lv_obj_create(NULL));
        lv_obj_t * new_scr = lv_obj_create(NULL);
        
        // 传NULL就是新屏幕，lv_scr_load切换到新屏幕
        lv_scr_load(new_scr);
        // 删除旧屏幕和键盘，节省空间，不删也行
        lv_obj_clean(old_scr);
        // 切换屏幕了就加载电子相册功能
        photo_album_create();

    } else {
        lv_label_set_text(status_label, "账号或密码错误");
    }
}


void login_screen_create(void)
{
    // ---- 清空屏幕 ----
    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_make(30, 30, 30), 0);

    // ---- 创建拼音输入法键盘 ----
    // 键盘默认在屏幕底部，占 55% 高度，宽度铺满
    // 输入框要放上半部分，不然会被键盘挡住
    lv_obj_t * pinyin_ime = lv_100ask_pinyin_ime_create(scr);
    lv_obj_set_style_text_font(pinyin_ime, &lv_font_source_han_sans_normal_16, 0);

    kb = lv_100ask_pinyin_ime_get_kb(pinyin_ime);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 一开始隐藏

    // ---- 标题 ----
    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "用户登录");
    lv_obj_set_style_text_font(title, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // ---- 账号区域 ----
    lv_obj_t * label_account = lv_label_create(scr);
    lv_label_set_text(label_account, "账号：");
    lv_obj_set_style_text_font(label_account, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_set_style_text_color(label_account, lv_color_white(), 0);
    lv_obj_align(label_account, LV_ALIGN_TOP_LEFT, 180, 50);

    ta_account = lv_textarea_create(scr);
    lv_obj_set_style_text_font(ta_account, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_set_size(ta_account, 300, 40);
    lv_textarea_set_one_line(ta_account, true);      // 单行输入
    lv_textarea_set_max_length(ta_account, 20);       // 最多 20 个字符
    lv_obj_align_to(ta_account, label_account, LV_ALIGN_OUT_RIGHT_TOP, 10, -5);
    // 点击输入框时弹出键盘，按完成/取消时收起，要不要都行
    lv_obj_add_event_cb(ta_account, ta_event_cb, LV_EVENT_ALL, kb);

    // ---- 密码区域 ----
    lv_obj_t * label_password = lv_label_create(scr);
    lv_label_set_text(label_password, "密码：");
    lv_obj_set_style_text_font(label_password, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_set_style_text_color(label_password, lv_color_white(), 0);
    lv_obj_align(label_password, LV_ALIGN_TOP_LEFT, 180, 100);

    ta_password = lv_textarea_create(scr);
    lv_obj_set_style_text_font(ta_password, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_set_size(ta_password, 300, 40);
    lv_textarea_set_one_line(ta_password, true);
    lv_textarea_set_max_length(ta_password, 20);
    lv_textarea_set_password_mode(ta_password, true);  // 密码模式（显示为 ***）
    lv_obj_align_to(ta_password, label_password, LV_ALIGN_OUT_RIGHT_TOP, 10, -5);
    // 点击输入框时弹出键盘，按完成/取消时收起
    lv_obj_add_event_cb(ta_password, ta_event_cb, LV_EVENT_ALL, kb);

    // ---- 登录按钮 ----
    lv_obj_t * login_btn = lv_btn_create(scr);
    lv_obj_set_size(login_btn, 160, 45);
    lv_obj_set_style_bg_color(login_btn, lv_color_make(0, 120, 200), 0);
    lv_obj_set_style_bg_color(login_btn, lv_color_make(0, 80, 160), LV_STATE_PRESSED);
    lv_obj_set_style_radius(login_btn, 8, 0);
    lv_obj_align(login_btn, LV_ALIGN_CENTER, 0, 160);
    lv_obj_add_event_cb(login_btn, login_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * login_text = lv_label_create(login_btn);
    lv_label_set_text(login_text, "登 录");
    lv_obj_set_style_text_font(login_text, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_center(login_text);

    // ---- 状态提示 ----
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_set_style_text_color(status_label, lv_color_make(255, 100, 100), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 210);

    // 将键盘的图层设置为屏幕最上层
    lv_obj_move_foreground(kb);
}
