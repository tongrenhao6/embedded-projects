#include "lv_data_demo.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ph_mqtt_client.h"
#include "ph_sampler.h"
#include "ph_storage_sd.h"
#include "ph_time.h"
#include "ph_wifi.h"

LV_FONT_DECLARE(myFont12)
LV_FONT_DECLARE(myFont24)
extern lv_indev_t *indev_keypad;

static lv_obj_t *s_data_obj;
static lv_style_t s_btn_style;
static lv_style_t s_btn_focus_style;
static lv_style_t s_btn_pressed_style;
static bool s_style_inited;
static lv_obj_t *s_first_btn;
static lv_obj_t *s_result_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_confirm_box;
static lv_group_t *s_confirm_group;
static lv_timer_t *s_result_timer;
static volatile bool s_action_busy;
static volatile bool s_result_ready;
static char s_result_text[96];
static char s_upload_info_text[32] = "未传:-- 行:--";
static uint8_t s_status_tick;
static int s_selected_year;
static int s_selected_month;
static int s_selected_day;

typedef enum {
    DATA_ACTION_SAMPLE_NOW = 1,
    DATA_ACTION_SEARCH_DATE,
    DATA_ACTION_DELETE_ONE,
    DATA_ACTION_DELETE_DATE,
    DATA_ACTION_DELETE_MONTH,
    DATA_ACTION_DELETE_UNKNOWN,
    DATA_ACTION_UPLOAD_RESERVED,
    DATA_ACTION_PREV_DAY,
    DATA_ACTION_NEXT_DAY,
} data_action_t;

static data_action_t s_pending_delete_action;

static const char *sampler_status_cn(const char *status);

#define DATA_ACTION_PACK(action, year, month, day) \
    ((((uint32_t)(action) & 0x0F) << 28) | (((uint32_t)(year) & 0x0FFF) << 16) | \
     (((uint32_t)(month) & 0x0F) << 8) | ((uint32_t)(day) & 0xFF))
#define DATA_ACTION_UNPACK_ACTION(value) ((data_action_t)(((uint32_t)(value) >> 28) & 0x0F))
#define DATA_ACTION_UNPACK_YEAR(value)   (int)(((uint32_t)(value) >> 16) & 0x0FFF)
#define DATA_ACTION_UNPACK_MONTH(value)  (int)(((uint32_t)(value) >> 8) & 0x0F)
#define DATA_ACTION_UNPACK_DAY(value)    (int)((uint32_t)(value) & 0xFF)

static void init_data_styles(void)
{
    if (s_style_inited) {
        return;
    }

    lv_style_init(&s_btn_style);
    lv_style_set_radius(&s_btn_style, 6);
    lv_style_set_bg_color(&s_btn_style, lv_color_hex(0x2C3948));
    lv_style_set_bg_opa(&s_btn_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_btn_style, 1);
    lv_style_set_border_color(&s_btn_style, lv_color_hex(0x5E748A));
    lv_style_set_text_color(&s_btn_style, lv_color_hex(0xFFFFFF));

    lv_style_init(&s_btn_focus_style);
    lv_style_set_bg_color(&s_btn_focus_style, lv_color_hex(0x0078D4));
    lv_style_set_border_width(&s_btn_focus_style, 3);
    lv_style_set_border_color(&s_btn_focus_style, lv_color_hex(0xFFFFFF));
    lv_style_set_outline_width(&s_btn_focus_style, 3);
    lv_style_set_outline_color(&s_btn_focus_style, lv_color_hex(0xFFC857));
    lv_style_set_outline_pad(&s_btn_focus_style, 2);

    lv_style_init(&s_btn_pressed_style);
    lv_style_set_bg_color(&s_btn_pressed_style, lv_color_hex(0x005A9E));

    s_style_inited = true;
}

static void lv_data_del(void)
{
    if (s_confirm_box != NULL) {
        lv_obj_del(s_confirm_box);
        s_confirm_box = NULL;
    }
    if (s_confirm_group != NULL) {
        lv_indev_set_group(indev_keypad, ctrl_g);
        lv_group_del(s_confirm_group);
        s_confirm_group = NULL;
    }
    if (s_data_obj != NULL) {
        lv_obj_clean(s_data_obj);
        lv_obj_del(s_data_obj);
        s_data_obj = NULL;
    }
    s_result_label = NULL;
    s_date_label = NULL;
    s_status_label = NULL;
    if (s_result_timer != NULL) {
        lv_timer_del(s_result_timer);
        s_result_timer = NULL;
    }

    app_obj_general.current_parent = NULL;
    app_obj_general.del_parent = NULL;
    lv_app_show();
}

static void set_result_text(const char *text)
{
    if (s_result_label != NULL) {
        lv_label_set_text(s_result_label, text);
    }
}

static void update_status_label(void)
{
    ph_sample_t last_sample;
    char last_text[32];

    if (s_status_label == NULL) {
        return;
    }

    if (ph_sampler_get_last_sample(&last_sample)) {
        snprintf(last_text, sizeof(last_text), "PH %.2f T %.1f",
                 last_sample.ph,
                 last_sample.temperature);
    } else {
        snprintf(last_text, sizeof(last_text), "PH -- T --");
    }

    lv_label_set_text_fmt(s_status_label,
                          "SD:%s WiFi:%s 时间:%s\n采样:%s %s %s",
                          ph_storage_is_ready() ? "OK" : "ERR",
                          ph_wifi_is_connected() ? "OK" : ph_wifi_get_status(),
                          ph_time_is_valid() ? "OK" : "WAIT",
                          sampler_status_cn(ph_sampler_get_status()),
                          last_text,
                          s_upload_info_text);
}

static void close_delete_confirm(void)
{
    if (s_confirm_box != NULL) {
        lv_obj_del(s_confirm_box);
        s_confirm_box = NULL;
    }

    if (s_confirm_group != NULL) {
        lv_indev_set_group(indev_keypad, ctrl_g);
        lv_group_del(s_confirm_group);
        s_confirm_group = NULL;
    }

    if (s_data_obj != NULL) {
        app_obj_general.Function = lv_data_del;
    }
}

static void update_date_label(void)
{
    if (s_date_label != NULL) {
        lv_label_set_text_fmt(s_date_label, "%04d-%02d-%02d",
                              s_selected_year,
                              s_selected_month,
                              s_selected_day);
    }
}

static void set_selected_date_from_time(time_t timestamp)
{
    struct tm timeinfo;

    localtime_r(&timestamp, &timeinfo);
    s_selected_year = timeinfo.tm_year + 1900;
    s_selected_month = timeinfo.tm_mon + 1;
    s_selected_day = timeinfo.tm_mday;
}

static void move_selected_date(int day_offset)
{
    struct tm timeinfo = {0};
    time_t timestamp;

    timeinfo.tm_year = s_selected_year - 1900;
    timeinfo.tm_mon = s_selected_month - 1;
    timeinfo.tm_mday = s_selected_day + day_offset;
    timeinfo.tm_isdst = -1;
    timestamp = mktime(&timeinfo);
    set_selected_date_from_time(timestamp);
    update_date_label();
}

static void set_async_result(const char *text)
{
    snprintf(s_result_text, sizeof(s_result_text), "%s", text);
    s_result_ready = true;
}

static void refresh_upload_info_in_worker(int year, int month)
{
    ph_upload_cursor_t cursor;
    char file_name[PH_UPLOAD_CURSOR_FILE_MAX];
    uint32_t pending_count = 0;

    memset(&cursor, 0, sizeof(cursor));
    snprintf(file_name, sizeof(file_name), "ph_%04d_%02d.csv", year, month);
    snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);

    if (ph_storage_count_pending_upload(year, month, &pending_count) != ESP_OK ||
        ph_upload_cursor_load(&cursor) != ESP_OK) {
        snprintf(s_upload_info_text, sizeof(s_upload_info_text), "未传:-- 行:--");
        return;
    }

    snprintf(s_upload_info_text, sizeof(s_upload_info_text),
             "未传:%lu 行:%lu",
             (unsigned long)pending_count,
             (unsigned long)cursor.line);
}

static void data_result_timer(lv_timer_t *timer)
{
    (void)timer;

    if (s_result_ready) {
        s_result_ready = false;
        set_result_text(s_result_text);
        update_status_label();
    }

    if (++s_status_tick >= 5) {
        s_status_tick = 0;
        update_status_label();
    }
}

static void data_action_worker(void *arg)
{
    uint32_t packed = (uint32_t)(uintptr_t)arg;
    data_action_t action = DATA_ACTION_UNPACK_ACTION(packed);
    int year = DATA_ACTION_UNPACK_YEAR(packed);
    int month = DATA_ACTION_UNPACK_MONTH(packed);
    int day = DATA_ACTION_UNPACK_DAY(packed);
    uint32_t count = 0;
    char msg[96];

    switch (action) {
        case DATA_ACTION_SAMPLE_NOW:
            if (ph_sampler_take_now() == ESP_OK) {
                set_async_result("采样已保存");
            } else {
                snprintf(msg, sizeof(msg), "采样失败:%s", ph_sampler_get_status());
                set_async_result(msg);
            }
            break;

        case DATA_ACTION_SEARCH_DATE:
            if (ph_storage_count_date(year, month, day, &count) == ESP_OK) {
                snprintf(msg, sizeof(msg), "选中日期:%lu", (unsigned long)count);
                set_async_result(msg);
            } else {
                snprintf(msg, sizeof(msg), "查询失败:%s", ph_storage_get_last_error());
                set_async_result(msg);
            }
            break;

        case DATA_ACTION_DELETE_ONE:
            if (ph_storage_delete_last_record(year, month, day, &count) == ESP_OK) {
                snprintf(msg, sizeof(msg), "已删最后一条:%lu", (unsigned long)count);
                set_async_result(msg);
            } else {
                snprintf(msg, sizeof(msg), "删除失败:%s", ph_storage_get_last_error());
                set_async_result(msg);
            }
            break;

        case DATA_ACTION_DELETE_DATE:
            if (ph_storage_delete_date(year, month, day, &count) == ESP_OK) {
                snprintf(msg, sizeof(msg), "已删当天:%lu", (unsigned long)count);
                set_async_result(msg);
            } else {
                snprintf(msg, sizeof(msg), "删除失败:%s", ph_storage_get_last_error());
                set_async_result(msg);
            }
            break;

        case DATA_ACTION_DELETE_MONTH:
            if (ph_storage_delete_month(year, month) == ESP_OK) {
                set_async_result("已删整月");
            } else {
                snprintf(msg, sizeof(msg), "删除失败:%s", ph_storage_get_last_error());
                set_async_result(msg);
            }
            break;

        case DATA_ACTION_DELETE_UNKNOWN:
            if (ph_storage_delete_unknown(&count) == ESP_OK) {
                snprintf(msg, sizeof(msg), "已删未知时间:%lu", (unsigned long)count);
                set_async_result(msg);
                if (ph_mqtt_client_is_connected()) {
                    ph_mqtt_publish_upload_status_current(NULL);
                }
            } else {
                snprintf(msg, sizeof(msg), "删除失败:%s", ph_storage_get_last_error());
                set_async_result(msg);
            }
            break;

        case DATA_ACTION_UPLOAD_RESERVED:
            if (ph_mqtt_upload_month(year, month, &count) == ESP_OK) {
                snprintf(msg, sizeof(msg), "MQTT:%lu", (unsigned long)count);
                set_async_result(msg);
            } else {
                snprintf(msg, sizeof(msg), "上传失败:%s", ph_mqtt_client_get_last_error());
                set_async_result(msg);
            }
            break;

        default:
            set_async_result("未知操作");
            break;
    }

    refresh_upload_info_in_worker(year, month);
    s_action_busy = false;
    vTaskDelete(NULL);
}

static void start_data_action(data_action_t action)
{
    BaseType_t ret;
    uint32_t packed;

    if (s_action_busy) {
        set_result_text("正在处理，请稍候");
        return;
    }

    switch (action) {
        case DATA_ACTION_SAMPLE_NOW:
            set_result_text("正在采样...");
            break;
        case DATA_ACTION_SEARCH_DATE:
            set_result_text("正在查询...");
            break;
        case DATA_ACTION_DELETE_ONE:
            set_result_text("正在删除最后一条...");
            break;
        case DATA_ACTION_DELETE_DATE:
            set_result_text("正在删除当天...");
            break;
        case DATA_ACTION_DELETE_MONTH:
            set_result_text("正在删除整月...");
            break;
        case DATA_ACTION_DELETE_UNKNOWN:
            set_result_text("正在删除未知时间...");
            break;
        case DATA_ACTION_UPLOAD_RESERVED:
            set_result_text("正在上传...");
            break;
        default:
            set_result_text("未知操作");
            break;
    }

    packed = DATA_ACTION_PACK(action, s_selected_year, s_selected_month, s_selected_day);
    s_action_busy = true;
    ret = xTaskCreatePinnedToCore(data_action_worker,
                                  "data_action",
                                  12288,
                                  (void *)(uintptr_t)packed,
                                  3,
                                  NULL,
                                  1);
    if (ret != pdPASS) {
        s_action_busy = false;
        set_result_text("任务创建失败");
    }
}

static bool is_delete_action(data_action_t action)
{
    return action == DATA_ACTION_DELETE_ONE ||
           action == DATA_ACTION_DELETE_DATE ||
           action == DATA_ACTION_DELETE_MONTH ||
           action == DATA_ACTION_DELETE_UNKNOWN;
}

static const char *delete_action_text(data_action_t action)
{
    switch (action) {
        case DATA_ACTION_DELETE_ONE:
            return "删除选中日期最后一条记录？";
        case DATA_ACTION_DELETE_DATE:
            return "删除选中日期全部记录？";
        case DATA_ACTION_DELETE_MONTH:
            return "删除选中月份全部记录？";
        case DATA_ACTION_DELETE_UNKNOWN:
            return "删除所有未知时间记录？";
        default:
            return "确认删除？";
    }
}

static void delete_confirm_event(lv_event_t *event)
{
    bool confirmed;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    confirmed = (bool)(uintptr_t)lv_event_get_user_data(event);
    close_delete_confirm();

    if (confirmed) {
        start_data_action(s_pending_delete_action);
    } else {
        set_result_text("已取消删除");
    }
}

static void show_delete_confirm(data_action_t action)
{
    lv_obj_t *title;
    lv_obj_t *text;
    lv_obj_t *cancel_btn;
    lv_obj_t *confirm_btn;
    lv_obj_t *label;

    close_delete_confirm();

    s_pending_delete_action = action;
    s_confirm_group = lv_group_create();
    if (s_confirm_group == NULL) {
        set_result_text("确认框创建失败");
        return;
    }

    s_confirm_box = lv_obj_create(lv_scr_act());
    if (s_confirm_box == NULL) {
        close_delete_confirm();
        set_result_text("确认框创建失败");
        return;
    }

    lv_obj_set_size(s_confirm_box, lcd_self.width - 28, 134);
    lv_obj_center(s_confirm_box);
    lv_obj_set_style_radius(s_confirm_box, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_confirm_box, lv_color_hex(0x202A35), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_confirm_box, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(s_confirm_box, lv_color_hex(0xAFC7DE), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_confirm_box, 2, LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_confirm_box, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(s_confirm_box);
    lv_label_set_text(title, "确认");
    lv_obj_set_style_text_font(title, &myFont12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    text = lv_label_create(s_confirm_box);
    lv_label_set_text(text, delete_action_text(action));
    lv_obj_set_style_text_font(text, &myFont12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(text, lv_color_hex(0xFFE6A6), LV_STATE_DEFAULT);
    lv_obj_set_width(text, lcd_self.width - 44);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_align(text, LV_ALIGN_TOP_MID, 0, 42);

    cancel_btn = lv_btn_create(s_confirm_box);
    lv_obj_set_size(cancel_btn, 82, 32);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_LEFT, 18, -12);
    lv_obj_add_style(cancel_btn, &s_btn_style, LV_STATE_DEFAULT);
    lv_obj_add_style(cancel_btn, &s_btn_focus_style, LV_STATE_FOCUSED);
    lv_obj_add_style(cancel_btn, &s_btn_pressed_style, LV_STATE_PRESSED);
    lv_obj_add_event_cb(cancel_btn, delete_confirm_event, LV_EVENT_CLICKED, (void *)0);
    lv_group_add_obj(s_confirm_group, cancel_btn);
    label = lv_label_create(cancel_btn);
    lv_label_set_text(label, "取消");
    lv_obj_set_style_text_font(label, &myFont12, LV_STATE_DEFAULT);
    lv_obj_center(label);

    confirm_btn = lv_btn_create(s_confirm_box);
    lv_obj_set_size(confirm_btn, 82, 32);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_RIGHT, -18, -12);
    lv_obj_add_style(confirm_btn, &s_btn_style, LV_STATE_DEFAULT);
    lv_obj_add_style(confirm_btn, &s_btn_focus_style, LV_STATE_FOCUSED);
    lv_obj_add_style(confirm_btn, &s_btn_pressed_style, LV_STATE_PRESSED);
    lv_obj_add_event_cb(confirm_btn, delete_confirm_event, LV_EVENT_CLICKED, (void *)1);
    lv_group_add_obj(s_confirm_group, confirm_btn);
    label = lv_label_create(confirm_btn);
    lv_label_set_text(label, "确认");
    lv_obj_set_style_text_font(label, &myFont12, LV_STATE_DEFAULT);
    lv_obj_center(label);

    lv_indev_set_group(indev_keypad, s_confirm_group);
    lv_group_focus_obj(cancel_btn);
    app_obj_general.Function = close_delete_confirm;
}

static void data_action_event(lv_event_t *event)
{
    data_action_t action = (data_action_t)(uintptr_t)lv_event_get_user_data(event);

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (action == DATA_ACTION_PREV_DAY) {
        move_selected_date(-1);
        update_status_label();
        set_result_text("日期已前移");
        return;
    }

    if (action == DATA_ACTION_NEXT_DAY) {
        move_selected_date(1);
        update_status_label();
        set_result_text("日期已后移");
        return;
    }

    if (is_delete_action(action)) {
        show_delete_confirm(action);
        return;
    }

    start_data_action(action);
}

static const char *sampler_status_cn(const char *status)
{
    if (status == NULL) {
        return "未知";
    }
    if (strcmp(status, "INIT") == 0) {
        return "初始化";
    }
    if (strcmp(status, "STARTED") == 0) {
        return "已启动";
    }
    if (strcmp(status, "WAIT_TIME") == 0) {
        return "等时间";
    }
    if (strcmp(status, "IDLE") == 0) {
        return "空闲";
    }
    if (strcmp(status, "SAMPLING") == 0) {
        return "采集中";
    }
    if (strcmp(status, "SAVED") == 0) {
        return "已保存";
    }
    if (strcmp(status, "SAVE_FAIL") == 0) {
        return "保存失败";
    }
    if (strcmp(status, "SENSOR_FAIL") == 0) {
        return "传感器失败";
    }
    if (strcmp(status, "BUSY") == 0) {
        return "采样忙";
    }
    if (strcmp(status, "DIRECT_UPLOADED") == 0) {
        return "直传成功";
    }
    if (strcmp(status, "DIRECT_FAIL") == 0) {
        return "直传失败";
    }
    if (strcmp(status, "NO_MEM") == 0) {
        return "内存不足";
    }

    return status;
}

static lv_obj_t *create_grid_btn(lv_obj_t *parent, const char *label, data_action_t action, int x, int y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *btn_label;

    lv_obj_set_size(btn, (lcd_self.width - 52) / 2, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_add_style(btn, &s_btn_style, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &s_btn_focus_style, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &s_btn_pressed_style, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, data_action_event, LV_EVENT_CLICKED, (void *)(uintptr_t)action);
    lv_group_add_obj(ctrl_g, btn);

    btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, label);
    lv_obj_set_style_text_font(btn_label, &myFont12, LV_STATE_DEFAULT);
    lv_obj_center(btn_label);

    return btn;
}

static lv_obj_t *create_small_btn(lv_obj_t *parent, const char *label, data_action_t action, int x, int y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *btn_label;

    lv_obj_set_size(btn, (lcd_self.width - 54) / 2, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_add_style(btn, &s_btn_style, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &s_btn_focus_style, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &s_btn_pressed_style, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, data_action_event, LV_EVENT_CLICKED, (void *)(uintptr_t)action);
    lv_group_add_obj(ctrl_g, btn);

    btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, label);
    lv_obj_set_style_text_font(btn_label, &myFont12, LV_STATE_DEFAULT);
    lv_obj_center(btn_label);

    return btn;
}

void lv_data_demo(void)
{
    lv_obj_t *title;

    init_data_styles();
    s_first_btn = NULL;
    s_result_label = NULL;
    s_date_label = NULL;
    s_status_label = NULL;
    s_result_ready = false;
    s_status_tick = 0;
    set_selected_date_from_time(ph_time_now());

    if (app_obj_general.current_parent != NULL) {
        if (s_result_timer != NULL) {
            lv_timer_del(s_result_timer);
            s_result_timer = NULL;
        }
        lv_obj_del(app_obj_general.current_parent);
        app_obj_general.current_parent = NULL;
        s_data_obj = NULL;
        s_result_label = NULL;
        s_date_label = NULL;
        s_status_label = NULL;
    }

    lv_app_del();

    s_data_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_style_radius(s_data_obj, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_data_obj, lv_color_hex(0x101820), LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(s_data_obj, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_size(s_data_obj, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
    lv_obj_set_pos(s_data_obj, 0, 20);
    lv_obj_clear_flag(s_data_obj, LV_OBJ_FLAG_SCROLLABLE);

    app_obj_general.current_parent = s_data_obj;
    app_obj_general.del_parent = s_data_obj;
    app_obj_general.Function = lv_data_del;

    title = lv_label_create(s_data_obj);
    lv_label_set_text(title, "PH数据管理");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &myFont12, LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    s_status_label = lv_label_create(s_data_obj);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xD0D7DF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_status_label, &myFont12, LV_STATE_DEFAULT);
    lv_obj_set_width(s_status_label, lcd_self.width - 16);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 238);

    s_date_label = lv_label_create(s_data_obj);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_18, LV_STATE_DEFAULT);
    lv_obj_align(s_date_label, LV_ALIGN_TOP_MID, 0, 28);
    update_date_label();

    s_first_btn = create_small_btn(s_data_obj, "前一天", DATA_ACTION_PREV_DAY, 18, 56);
    create_small_btn(s_data_obj, "后一天", DATA_ACTION_NEXT_DAY, 126, 56);
    create_grid_btn(s_data_obj, "立即采样", DATA_ACTION_SAMPLE_NOW, 18, 94);
    create_grid_btn(s_data_obj, "查询", DATA_ACTION_SEARCH_DATE, 126, 94);
    create_grid_btn(s_data_obj, "上传", DATA_ACTION_UPLOAD_RESERVED, 18, 132);
    create_grid_btn(s_data_obj, "删未知", DATA_ACTION_DELETE_UNKNOWN, 126, 132);

    s_result_label = lv_label_create(s_data_obj);
    lv_label_set_text(s_result_label, "请选择操作");
    lv_obj_set_style_text_color(s_result_label, lv_color_hex(0xFFC857), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_result_label, &myFont12, LV_STATE_DEFAULT);
    lv_obj_set_width(s_result_label, lcd_self.width - 24);
    lv_obj_set_style_text_align(s_result_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_align(s_result_label, LV_ALIGN_TOP_MID, 0, 212);
    s_result_timer = lv_timer_create(data_result_timer, 200, NULL);
    update_status_label();

    if (s_first_btn != NULL) {
        lv_group_focus_obj(s_first_btn);
    }
}
