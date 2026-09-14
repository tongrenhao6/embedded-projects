/* 
    客户端界面程序 负责 LVGL 界面和按钮逻辑
*/
#include "ui_order.h"
#include "order_client.h"

#include <stdio.h>

/* The full Chinese font is already compiled by the project's Makefile. */
LV_FONT_DECLARE(lv_font_source_han_sans_normal_16);
LV_FONT_DECLARE(ui_font_test1);
LV_IMG_DECLARE(ui_img_order_cart);
LV_IMG_DECLARE(ui_img_order_checkout);
LV_IMG_DECLARE(ui_img_585191297);
LV_IMG_DECLARE(ui_img_food_chicken_rice);
LV_IMG_DECLARE(ui_img_food_beef_noodles);
LV_IMG_DECLARE(ui_img_food_pork_feet_rice);
LV_IMG_DECLARE(ui_img_food_fried_chicken);
LV_IMG_DECLARE(ui_img_food_french_fries);
LV_IMG_DECLARE(ui_img_food_grilled_skewers);
LV_IMG_DECLARE(ui_img_food_mineral_water);
LV_IMG_DECLARE(ui_img_food_lemon_tea);
LV_IMG_DECLARE(ui_img_food_hot_milk);
LV_IMG_DECLARE(ui_img_food_cream_puff);
LV_IMG_DECLARE(ui_img_food_pudding);
LV_IMG_DECLARE(ui_img_food_cake);

#define ORDER_CATEGORY_COUNT 5                  // 菜品类别数量
#define ORDER_DISH_COUNT     12                 // 菜品数量
#define ORDER_CARD_COUNT     ORDER_DISH_COUNT   // 购物车卡片数量
#define ORDER_SERVER_IP      "118.31.185.167"   // 服务器 IP 地址
#define ORDER_SERVER_PORT    18080              // 服务器端口号

typedef enum {
    ORDER_CATEGORY_ALL,
    ORDER_CATEGORY_MAIN,
    ORDER_CATEGORY_SNACK,
    ORDER_CATEGORY_DRINK,
    ORDER_CATEGORY_DESSERT
} order_category_t;

typedef struct {
    int id;             // 菜品ID
    char name[64];      // 菜品名称
    char category[32];  // 菜品类别
    float price;        // 菜品价格
    order_category_t category_id;   // 菜品类别ID
    int quantity;                   // 菜品数量
    const lv_img_dsc_t *image;      // 菜品图片
    uint32_t food_color;            // 菜品颜色     //对应选中菜品颜色
    uint32_t food_color_2;          // 菜品颜色2    //对应未选中菜品颜色
} order_dish_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *name_label;
    lv_obj_t *price_label;
    lv_obj_t *quantity_label;
    lv_obj_t *food_image;
    int dish_index;
} order_card_t;

static const char *const category_names[ORDER_CATEGORY_COUNT] = {
    "全部", "主食", "小吃", "饮品", "甜品"
};

/* These values match server/data/menu.txt and are only used before TCP integration. */
static order_dish_t dishes[ORDER_DISH_COUNT] = {
    {1, "鸡腿饭", "主食", 12.0f, ORDER_CATEGORY_MAIN, 0, &ui_img_food_chicken_rice, 0xFFCC78, 0xB9572F},
    {2, "牛肉面", "主食", 15.0f, ORDER_CATEGORY_MAIN, 0, &ui_img_food_beef_noodles, 0xE0B970, 0x864B2B},
    {3, "猪脚饭", "主食", 18.0f, ORDER_CATEGORY_MAIN, 0, &ui_img_food_pork_feet_rice, 0xE7B675, 0x9E5430},
    {4, "炸鸡", "小吃", 8.0f, ORDER_CATEGORY_SNACK, 0, &ui_img_food_fried_chicken, 0xFFCB71, 0xDC7930},
    {5, "薯条", "小吃", 5.0f, ORDER_CATEGORY_SNACK, 0, &ui_img_food_french_fries, 0xFFD765, 0xE49A28},
    {6, "烤串", "小吃", 3.0f, ORDER_CATEGORY_SNACK, 0, &ui_img_food_grilled_skewers, 0xEDB57A, 0x9B502A},
    {7, "矿泉水", "饮品", 2.0f, ORDER_CATEGORY_DRINK, 0, &ui_img_food_mineral_water, 0xBDE8F0, 0x5CABC3},
    {8, "柠檬茶", "饮品", 5.0f, ORDER_CATEGORY_DRINK, 0, &ui_img_food_lemon_tea, 0xFFDE64, 0x6BA84E},
    {9, "热牛奶", "饮品", 4.0f, ORDER_CATEGORY_DRINK, 0, &ui_img_food_hot_milk, 0xF9EDC9, 0xDEAE6E},
    {10, "泡芙", "甜品", 3.5f, ORDER_CATEGORY_DESSERT, 0, &ui_img_food_cream_puff, 0xFFDE91, 0xD09136},
    {11, "布丁", "甜品", 2.0f, ORDER_CATEGORY_DESSERT, 0, &ui_img_food_pudding, 0xFFE2A4, 0xD49C37},
    {12, "蛋糕", "甜品", 4.0f, ORDER_CATEGORY_DESSERT, 0, &ui_img_food_cake, 0xF4B2A9, 0xC96E67}
};

static lv_obj_t *category_buttons[ORDER_CATEGORY_COUNT];
static order_card_t order_cards[ORDER_CARD_COUNT];
static lv_obj_t *dish_list;
static lv_obj_t *order_screen;
static lv_obj_t *cart_screen;
static lv_obj_t *cart_list;
static lv_obj_t *cart_rows[ORDER_DISH_COUNT];
static lv_obj_t *cart_item_labels[ORDER_DISH_COUNT];
static lv_obj_t *cart_quantity_labels[ORDER_DISH_COUNT];
static lv_obj_t *cart_subtotal_labels[ORDER_DISH_COUNT];
static lv_obj_t *cart_screen_total_label;
static lv_obj_t *cart_screen_status_label;
static lv_obj_t *cart_count_label;
static lv_obj_t *cart_total_label;
static lv_obj_t *status_label;
static int selected_category = ORDER_CATEGORY_ALL;
static order_client_t order_client = { -1 };
static float server_total;

static void refresh_cart_screen(void);
static void cart_checkout_button_event_cb(lv_event_t *event);

/**
 * 函数功能：清除对象默认的内边距和行列间距。
 *
 * 本项目大量使用绝对坐标定位。清除默认间距后，LVGL 主题不会让控件的位置产生偏移。
 *
 * 参数：
 *   obj - 要清除内边距的 LVGL 对象。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void clear_layout_padding(lv_obj_t *obj)
{
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(obj, 0, LV_PART_MAIN);
}

/**
 * 函数功能：为标签设置中文字体和文字颜色。
 *
 * 参数：
 *   label - 要设置样式的标签对象。
 *   color - 标签文字颜色。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void style_chinese_label(lv_obj_t *label, lv_color_t color)
{
    lv_obj_set_style_text_font(label, &lv_font_source_han_sans_normal_16, 0);
    lv_obj_set_style_text_color(label, color, 0);
}

/**
 * 函数功能：创建一个中文标签，并设置文字、颜色和绝对坐标。
 *
 * 参数：
 *   parent - 标签的父对象。
 *   text   - 标签要显示的 UTF-8 文本。
 *   color  - 标签文字颜色。
 *   x      - 标签相对于父对象的横坐标。
 *   y      - 标签相对于父对象的纵坐标。
 *
 * 返回值：
 *   成功时返回新创建的 lv_obj_t 标签对象指针。
 */
static lv_obj_t *create_label(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    style_chinese_label(label, color);
    lv_obj_set_pos(label, x, y);
    return label;
}

/**
 * 函数功能：根据 selected_category 刷新左侧分类按钮的选中样式。
 *
 * 当前分类按钮会显示橙色背景和白色文字，未选中按钮保持浅色背景。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void refresh_category_buttons(void)
{
    int i;

    for(i = 0; i < ORDER_CATEGORY_COUNT; i++) {
        bool selected = (i == selected_category);

        lv_obj_set_style_bg_color(category_buttons[i],
                                  selected ? lv_color_hex(0xF46023) : lv_color_hex(0xFFFDF6),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(category_buttons[i],
                                       selected ? lv_color_hex(0xFF9D3F) : lv_color_hex(0xFFFDF6),
                                       LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(category_buttons[i], selected ? LV_GRAD_DIR_VER : LV_GRAD_DIR_NONE,
                                     LV_PART_MAIN);
        lv_obj_set_style_border_color(category_buttons[i],
                                      selected ? lv_color_hex(0xFFCD83) : lv_color_hex(0xFBE1BE),
                                      LV_PART_MAIN);
        lv_obj_set_style_text_color(category_buttons[i],
                                    selected ? lv_color_white() : lv_color_hex(0x5B3A27),
                                    LV_PART_MAIN);
        lv_obj_set_style_shadow_width(category_buttons[i], selected ? 10 : 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(category_buttons[i], lv_color_hex(0xE67034), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(category_buttons[i], selected ? LV_OPA_50 : LV_OPA_20, LV_PART_MAIN);
    }
}

/**
 * 函数功能：刷新点餐页底部购物车的菜品份数和总价文字。
 *
 * 份数通过遍历 dishes 数组中的 quantity 求和；总价使用服务器返回的 server_total。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void refresh_cart(void)
{
    int i;
    int count = 0;
    char text[48];

    for(i = 0; i < ORDER_DISH_COUNT; i++) {
        count += dishes[i].quantity;
    }

    lv_snprintf(text, sizeof(text), "已选 %d 份菜品", count);
    lv_label_set_text(cart_count_label, text);

    /* LVGL's snprintf disables float formatting in this project, so use libc here. */
    snprintf(text, sizeof(text), "合计 ￥%.2f", server_total);
    lv_label_set_text(cart_total_label, text);
}

/**
 * 函数功能：根据当前分类，刷新可滚动菜品列表中的卡片内容和显示状态。
 *
 * 该函数会把符合分类的菜品依次绑定到卡片上，更新名称、价格、数量和图片；
 * 不需要显示的卡片会被隐藏。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void refresh_cards(void)
{
    int i;
    int j = 0;

    for(i = 0; i < ORDER_DISH_COUNT && j < ORDER_CARD_COUNT; i++) {
        char text[24];

        if(selected_category != ORDER_CATEGORY_ALL && (int)dishes[i].category_id != selected_category) {
            continue;
        }

        order_cards[j].dish_index = i;
        lv_obj_clear_flag(order_cards[j].root, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(order_cards[j].name_label, dishes[i].name);
        snprintf(text, sizeof(text), "￥%.2f", dishes[i].price);
        lv_label_set_text(order_cards[j].price_label, text);
        lv_label_set_text_fmt(order_cards[j].quantity_label, "%d", dishes[i].quantity);

        if(dishes[i].image != NULL) {
            lv_img_set_src(order_cards[j].food_image, dishes[i].image);
            lv_obj_clear_flag(order_cards[j].food_image, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(order_cards[j].food_image, LV_OBJ_FLAG_HIDDEN);
        }
        j++;
    }

    while(j < ORDER_CARD_COUNT) {
        order_cards[j].dish_index = -1;
        lv_obj_add_flag(order_cards[j].root, LV_OBJ_FLAG_HIDDEN);
        j++;
    }
}

/**
 * 函数功能：统一刷新点餐页面和购物车页面的所有动态数据。
 *
 * 菜品数量、总价或分类改变后，调用本函数可避免分别遗漏某个界面区域。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void refresh_order_ui(void)
{
    refresh_category_buttons();  // 刷新分类按钮样式
    refresh_cards();             // 刷新菜品卡片内容
    refresh_cart();              // 刷新购物车底部文字
    refresh_cart_screen();       // 刷新购物车页面
}

/**
 * 函数功能：把服务器返回的中文分类名称转换为本地分类枚举值。
 *
 * 本地图片通过菜品 ID 绑定，名称、价格、分类则由服务器 MENU 响应更新；
 * 因此需要把“主食”等字符串转换为 ORDER_CATEGORY_MAIN 等枚举值。
 *
 * 参数：
 *   category - 服务器返回的分类名称字符串。
 *
 * 返回值：
 *   对应的 order_category_t 枚举值；未识别的分类返回 -1。
 */
static int category_id_from_name(const char *category)
{
    if(strcmp(category, "主食") == 0) {
        return ORDER_CATEGORY_MAIN;
    }
    if(strcmp(category, "小吃") == 0) {
        return ORDER_CATEGORY_SNACK;
    }
    if(strcmp(category, "饮品") == 0) {
        return ORDER_CATEGORY_DRINK;
    }
    if(strcmp(category, "甜品") == 0) {
        return ORDER_CATEGORY_DESSERT;
    }

    return -1;
}

/**
 * 函数功能：清空开发板本地保存的购物车数量和总价。
 *
 * 服务端结算成功或重新连接服务器后调用。本地只保存用于显示的数量，
 * 服务端链表购物车才是最终订单数据。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void clear_local_cart(void)
{
    int i;

    for(i = 0; i < ORDER_DISH_COUNT; i++) {
        dishes[i].quantity = 0;
    }
    server_total = 0.0f;
}

/**
 * 函数功能：使用服务器 MENU 响应更新开发板本地菜品的名称、分类和价格。
 *
 * 本地 dishes 数组保留固定菜品 ID 和图片指针；从服务器获取的菜单数据覆盖文字和价格，
 * 这样修改服务器 menu.txt 后，开发板无需重新写死价格和名称。
 *
 * 参数：
 *   menu       - 已由 order_client_load_menu() 解析出的菜单数组。
 *   menu_count - menu 数组中实际菜品数量。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void apply_server_menu(const order_menu_item_t menu[], size_t menu_count)
{
    size_t menu_index;
    int dish_index;

    for(menu_index = 0; menu_index < menu_count; menu_index++) {
        for(dish_index = 0; dish_index < ORDER_DISH_COUNT; dish_index++) {
            int category_id;

            if(dishes[dish_index].id != menu[menu_index].id) {
                continue;
            }

            snprintf(dishes[dish_index].name, sizeof(dishes[dish_index].name), "%s", menu[menu_index].name);
            snprintf(dishes[dish_index].category, sizeof(dishes[dish_index].category), "%s", menu[menu_index].category);
            dishes[dish_index].price = menu[menu_index].price;

            category_id = category_id_from_name(menu[menu_index].category);
            if(category_id >= 0) {
                dishes[dish_index].category_id = (order_category_t)category_id;
            }
            break;
        }
    }
}

/**
 * 函数功能：确认开发板当前已连接服务器；断开时尝试重新连接。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   0  - 当前连接可用，或重新连接成功。
 *   -1 - 重新连接服务器失败。
 */
static int ensure_server_connected(void)
{
    if(order_client.socket_fd >= 0) {
        return 0;
    }

    ui_order_connect_server();
    return order_client.socket_fd >= 0 ? 0 : -1;
}

/**
 * 函数功能：处理左侧分类按钮的点击事件。
 *
 * 事件绑定时将分类枚举值作为 user_data 保存。本函数取出该值，切换当前分类，
 * 把菜品列表滚回顶部，再刷新界面。
 *
 * 参数：
 *   event - LVGL 点击事件对象，user_data 中保存分类枚举值地址。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void category_button_event_cb(lv_event_t *event)
{
    const int *category = lv_event_get_user_data(event);

    selected_category = *category;
    if(dish_list != NULL) {
        lv_obj_scroll_to_y(dish_list, 0, LV_ANIM_OFF);
    }
    refresh_order_ui();
}

/**
 * 函数功能：处理菜品卡片“+”按钮的点击事件。
 *
 * 先向服务器发送 ADD|菜品ID 命令。只有服务器返回成功后，才增加本地显示数量，
 * 并使用服务器返回的总价刷新界面，保证开发板显示与服务器购物车一致。
 *
 * 参数：
 *   event - LVGL 点击事件对象，user_data 中保存当前菜品卡片结构体地址。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void add_button_event_cb(lv_event_t *event)
{
    order_card_t *card = lv_event_get_user_data(event);
    float total;

    if(card->dish_index >= 0 && card->dish_index < ORDER_DISH_COUNT &&
       ensure_server_connected() == 0 &&
       order_client_add(&order_client, dishes[card->dish_index].id, &total) == 0) {
        dishes[card->dish_index].quantity++;
        server_total = total;
        lv_label_set_text(status_label, "已连接");
        refresh_order_ui();// 更新显示数据
    } else {
        order_client_close(&order_client);
        lv_label_set_text(status_label, "添加失败");
    }
}

/**
 * 函数功能：处理菜品卡片“-”按钮的点击事件。
 *
 * 当本地数量大于 0 时，向服务器发送 REMOVE|菜品ID 命令；服务器成功响应后，
 * 再减少本地显示数量并更新总价。
 *
 * 参数：
 *   event - LVGL 点击事件对象，user_data 中保存当前菜品卡片结构体地址。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void remove_button_event_cb(lv_event_t *event)
{
    order_card_t *card = lv_event_get_user_data(event);
    float total;

    if(card->dish_index >= 0 && card->dish_index < ORDER_DISH_COUNT &&
       dishes[card->dish_index].quantity > 0 && ensure_server_connected() == 0 &&
       order_client_remove(&order_client, dishes[card->dish_index].id, &total) == 0) {
        dishes[card->dish_index].quantity--;
        server_total = total;
        lv_label_set_text(status_label, "已连接");
        refresh_order_ui();
    } else if(card->dish_index >= 0 && card->dish_index < ORDER_DISH_COUNT &&
              dishes[card->dish_index].quantity > 0) {
        order_client_close(&order_client);
        lv_label_set_text(status_label, "删除失败");
    }
}

/**
 * 函数功能：关闭下单成功弹窗。
 *
 * 参数：
 *   event - LVGL 点击事件对象。用户数据中保存了遮罩对象指针。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void order_success_popup_close_event_cb(lv_event_t *event)
{
    lv_obj_t *overlay = lv_event_get_user_data(event);

    lv_obj_del(overlay);
}

/**
 * 函数功能：在当前屏幕上显示下单成功弹窗。
 *
 * 弹窗使用半透明遮罩覆盖当前屏幕，显示服务端返回的订单号和实付金额。
 * 用户点击“确定”后，遮罩和弹窗会一起删除，回到正常点餐界面。
 *
 * 参数：
 *   order_id - 服务端生成的订单号。
 *   total    - 服务端确认的订单总金额。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void show_order_success_popup(unsigned int order_id, float total)
{
    lv_obj_t *overlay;
    lv_obj_t *dialog;
    lv_obj_t *title_label;
    lv_obj_t *order_label;
    lv_obj_t *total_label;
    lv_obj_t *confirm_button;
    lv_obj_t *confirm_label;
    char text[64];

    /* 遮罩覆盖当前屏幕，避免用户在弹窗显示时误触下面的按钮。 */
    overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, 800, 480);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    clear_layout_padding(overlay);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    dialog = lv_obj_create(overlay);
    lv_obj_set_size(dialog, 360, 210);
    lv_obj_center(dialog);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    clear_layout_padding(dialog);
    lv_obj_set_style_radius(dialog, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0xFFFDF6), LV_PART_MAIN);
    lv_obj_set_style_border_color(dialog, lv_color_hex(0xFFE1B9), LV_PART_MAIN);
    lv_obj_set_style_border_width(dialog, 1, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dialog, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(dialog, lv_color_hex(0x4B2F20), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(dialog, LV_OPA_40, LV_PART_MAIN);

    title_label = create_label(dialog, "下单成功", lv_color_hex(0xE5551E), 0, 26);
    lv_obj_set_width(title_label, 360);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    snprintf(text, sizeof(text), "订单号：%u", order_id);
    order_label = create_label(dialog, text, lv_color_hex(0x5B3A27), 0, 72);
    lv_obj_set_width(order_label, 360);
    lv_obj_set_style_text_align(order_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    snprintf(text, sizeof(text), "实付：%.2f 元", total);
    total_label = create_label(dialog, text, lv_color_hex(0xE5551E), 0, 105);
    lv_obj_set_width(total_label, 360);
    lv_obj_set_style_text_align(total_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    confirm_button = lv_btn_create(dialog);
    lv_obj_set_size(confirm_button, 128, 44);
    lv_obj_set_pos(confirm_button, 116, 148);
    lv_obj_set_style_radius(confirm_button, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(confirm_button, lv_color_hex(0xF45B19), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(confirm_button, lv_color_hex(0xFF9837), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(confirm_button, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_width(confirm_button, 0, LV_PART_MAIN);
    confirm_label = lv_label_create(confirm_button);
    lv_label_set_text(confirm_label, "确定");
    style_chinese_label(confirm_label, lv_color_white());
    lv_obj_center(confirm_label);
    lv_obj_add_event_cb(confirm_button, order_success_popup_close_event_cb, LV_EVENT_CLICKED, overlay);
}

/**
 * 函数功能：处理点餐页底部“去结算”按钮的点击事件。
 *
 * 函数会先检查购物车是否为空，再发送 SUBMIT 命令。服务端保存订单成功后，
 * 清空本地数量和总价，并刷新页面。
 *
 * 参数：
 *   event - LVGL 点击事件对象。本函数不需要读取其中的数据。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void checkout_button_event_cb(lv_event_t *event)
{
    int i;
    int count = 0;
    unsigned int order_id;
    float total;

    LV_UNUSED(event);

    for(i = 0; i < ORDER_DISH_COUNT; i++) {
        count += dishes[i].quantity;
    }

    if(count == 0) {
        lv_label_set_text(status_label, "购物车为空");
        return;
    }

    if(ensure_server_connected() != 0 ||
       order_client_submit(&order_client, &order_id, &total) != 0) {
        order_client_close(&order_client);
        lv_label_set_text(status_label, "提交失败");
        return;
    }

    /* The server has written this order to orders.txt and cleared its cart. */
    clear_local_cart();
    lv_label_set_text(status_label, "订单已提交");
    refresh_order_ui();
    show_order_success_popup(order_id, total);
}

/**
 * 函数功能：处理点餐页购物车区域的点击事件，打开购物车屏幕。
 *
 * 打开前先刷新购物车内容并把滚动列表回到顶部，保证用户首先看到已选菜品。
 *
 * 参数：
 *   event - LVGL 点击事件对象。本函数不需要读取其中的数据。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void cart_panel_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    refresh_cart_screen();
    lv_obj_scroll_to_y(cart_list, 0, LV_ANIM_OFF);
    lv_scr_load(cart_screen);
}

/**
 * 函数功能：处理购物车屏幕“返回点餐”按钮的点击事件。
 *
 * 只切换回点餐屏幕，不改变本地菜品数量和服务器购物车。
 *
 * 参数：
 *   event - LVGL 点击事件对象。本函数不需要读取其中的数据。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void cart_back_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    lv_scr_load(order_screen);
}

/**
 * 函数功能：刷新购物车屏幕中已选择菜品的行、数量、小计和总价。
 *
 * 未选择的菜品行会隐藏并移回起点；已选择菜品按连续行号从顶部排列，
 * 这样不会因为菜品 ID 较大而出现在列表底部。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void refresh_cart_screen(void)
{
    int i;
    int visible_index = 0;
    char text[64];

    if(cart_screen == NULL) {
        return;
    }

    for(i = 0; i < ORDER_DISH_COUNT; i++) {
        if(dishes[i].quantity <= 0) {
            /* 隐藏行移回起点，避免它继续撑大滚动区域。 */
            lv_obj_set_y(cart_rows[i], 0);
            lv_obj_add_flag(cart_rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        /* 只给已选择的菜品分配连续行号。 */
        lv_obj_set_y(cart_rows[i], visible_index * 52);     
        lv_obj_clear_flag(cart_rows[i], LV_OBJ_FLAG_HIDDEN);        
        lv_label_set_text(cart_item_labels[i], dishes[i].name);
        lv_label_set_text_fmt(cart_quantity_labels[i], "x%d", dishes[i].quantity);
        snprintf(text, sizeof(text), "%.2f", dishes[i].price * dishes[i].quantity);
        lv_label_set_text(cart_subtotal_labels[i], text);
        visible_index++;
    }

    snprintf(text, sizeof(text), "合计  %.2f 元", server_total);
    lv_label_set_text(cart_screen_total_label, text);
}

/**
 * 函数功能：创建购物车屏幕及其内部控件。
 *
 * 本函数只在点餐屏幕初始化时调用一次，创建背景、返回按钮、可滚动菜品列表、
 * 总价标签和结算按钮。之后打开购物车时只调用 refresh_cart_screen() 更新数据。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void create_cart_screen(void)
{
    lv_obj_t *background;
    lv_obj_t *back_button;
    lv_obj_t *back_label;
    lv_obj_t *checkout_button;
    lv_obj_t *label;
    int i;

    cart_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(cart_screen, LV_OBJ_FLAG_SCROLLABLE);
    clear_layout_padding(cart_screen);
    lv_obj_set_style_bg_opa(cart_screen, LV_OPA_TRANSP, LV_PART_MAIN);

    background = lv_img_create(cart_screen);
    lv_img_set_src(background, &ui_img_585191297);
    lv_obj_set_pos(background, 0, 0);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_SCROLLABLE);

    label = create_label(cart_screen, "购物车", lv_color_white(), 350, 20);
    lv_obj_set_style_text_font(label, &lv_font_source_han_sans_normal_16, 0);

    back_button = lv_btn_create(cart_screen);
    lv_obj_set_size(back_button, 110, 42);
    lv_obj_set_pos(back_button, 18, 9);
    lv_obj_set_style_radius(back_button, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0xFFFDF6), LV_PART_MAIN);
    lv_obj_set_style_border_width(back_button, 0, LV_PART_MAIN);
    back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, "返回点餐");
    style_chinese_label(back_label, lv_color_hex(0x5B3A27));
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_button, cart_back_event_cb, LV_EVENT_CLICKED, NULL);

    cart_list = lv_obj_create(cart_screen);
    lv_obj_set_size(cart_list, 740, 310);
    lv_obj_set_pos(cart_list, 30, 70);
    lv_obj_set_scroll_dir(cart_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cart_list, LV_SCROLLBAR_MODE_AUTO);
    clear_layout_padding(cart_list);
    lv_obj_set_style_bg_color(cart_list, lv_color_hex(0xFFFDF6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cart_list, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(cart_list, lv_color_hex(0xFFE7C8), LV_PART_MAIN);
    lv_obj_set_style_border_width(cart_list, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(cart_list, 14, LV_PART_MAIN);

    for(i = 0; i < ORDER_DISH_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(cart_list);

        cart_rows[i] = row;
        lv_obj_set_size(row, 700, 46);
        lv_obj_set_pos(row, 10, i * 52);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        clear_layout_padding(row);
        lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_color(row, i % 2 ? lv_color_hex(0xFFF7E9) : lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);

        cart_item_labels[i] = create_label(row, dishes[i].name, lv_color_hex(0x4B2F20), 18, 13);
        cart_quantity_labels[i] = create_label(row, "x0", lv_color_hex(0x7B5C45), 350, 13);
        cart_subtotal_labels[i] = create_label(row, "0.00", lv_color_hex(0xE04C18), 520, 13);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }

    cart_screen_total_label = create_label(cart_screen, "合计  0.00 元", lv_color_hex(0xE04C18), 360, 415);
    cart_screen_status_label = create_label(cart_screen, "", lv_color_hex(0x8F6F55), 30, 420);

    checkout_button = lv_btn_create(cart_screen);
    lv_obj_set_size(checkout_button, 160, 55);
    lv_obj_set_pos(checkout_button, 620, 400);
    lv_obj_set_style_bg_img_src(checkout_button, &ui_img_order_checkout, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(checkout_button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(checkout_button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(checkout_button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(checkout_button, cart_checkout_button_event_cb, LV_EVENT_CLICKED, NULL);
}

/**
 * 函数功能：处理购物车屏幕“去结算”按钮的点击事件。
 *
 * 先检查本地购物车是否为空，再向服务器发送 SUBMIT 命令。服务器成功保存订单后，
 * 清空本地显示数据、刷新点餐页，并自动返回点餐屏幕。
 *
 * 参数：
 *   event - LVGL 点击事件对象。本函数不需要读取其中的数据。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void cart_checkout_button_event_cb(lv_event_t *event)
{
    int i;
    int count = 0;
    unsigned int order_id;
    float total;

    LV_UNUSED(event);

    for(i = 0; i < ORDER_DISH_COUNT; i++) {
        count += dishes[i].quantity;
    }

    if(count == 0) {
        lv_label_set_text(cart_screen_status_label, "购物车为空");
        return;
    }

    if(ensure_server_connected() != 0 ||
       order_client_submit(&order_client, &order_id, &total) != 0) {
        order_client_close(&order_client);
        lv_label_set_text(cart_screen_status_label, "提交失败");
        return;
    }

    clear_local_cart();
    lv_label_set_text(status_label, "订单已提交");
    lv_label_set_text(cart_screen_status_label, "订单已提交");
    refresh_order_ui();
    lv_scr_load(order_screen);
    show_order_success_popup(order_id, total);
}

/**
 * 函数功能：为菜品卡片创建没有真实图片时使用的彩色占位图。
 *
 * 占位图由盘子、食物和点缀等 LVGL 图形对象组成。真实图片存在时，真实图片会覆盖在
 * 占位图上方；没有图片时仍可保持卡片界面完整。
 *
 * 参数：
 *   card - 要添加占位图的菜品卡片对象。
 *   dish - 当前菜品数据，用于读取占位图颜色。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void create_food_placeholder(lv_obj_t *card, const order_dish_t *dish)
{
    lv_obj_t *panel = lv_obj_create(card);
    lv_obj_t *plate;
    lv_obj_t *food_a;
    lv_obj_t *food_b;
    lv_obj_t *garnish;

    lv_obj_set_size(panel, 160, 104);
    lv_obj_set_pos(panel, 14, 13);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    clear_layout_padding(panel);
    lv_obj_set_style_radius(panel, 11, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel, lv_color_hex(dish->food_color), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(panel, lv_color_hex(0xFFF4DC), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);

    plate = lv_obj_create(panel);
    lv_obj_set_size(plate, 104, 50);
    lv_obj_set_pos(plate, 28, 35);
    lv_obj_clear_flag(plate, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(plate, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(plate, lv_color_hex(0xFFF9E5), LV_PART_MAIN);
    lv_obj_set_style_border_color(plate, lv_color_hex(0xF2D29F), LV_PART_MAIN);
    lv_obj_set_style_border_width(plate, 2, LV_PART_MAIN);

    food_a = lv_obj_create(panel);
    lv_obj_set_size(food_a, 43, 22);
    lv_obj_set_pos(food_a, 43, 49);
    lv_obj_clear_flag(food_a, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(food_a, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(food_a, lv_color_hex(dish->food_color_2), LV_PART_MAIN);
    lv_obj_set_style_border_width(food_a, 0, LV_PART_MAIN);

    food_b = lv_obj_create(panel);
    lv_obj_set_size(food_b, 34, 19);
    lv_obj_set_pos(food_b, 79, 54);
    lv_obj_clear_flag(food_b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(food_b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(food_b, lv_color_hex(0xEE9F47), LV_PART_MAIN);
    lv_obj_set_style_border_width(food_b, 0, LV_PART_MAIN);

    garnish = lv_obj_create(panel);
    lv_obj_set_size(garnish, 12, 12);
    lv_obj_set_pos(garnish, 105, 42);
    lv_obj_clear_flag(garnish, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(garnish, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(garnish, lv_color_hex(0x669F4A), LV_PART_MAIN);
    lv_obj_set_style_border_width(garnish, 0, LV_PART_MAIN);
}

/**
 * 函数功能：创建一张菜品卡片及其图片、名称、价格、数量、“+”和“-”按钮。
 *
 * 卡片创建后先不绑定筛选结果；refresh_cards() 会根据当前分类把实际菜品数据填入卡片。
 * 加减按钮通过 user_data 保存 card_data，从而知道自己对应哪一道菜。
 *
 * 参数：
 *   parent     - 菜品卡片的父对象，即可滚动的 dish_list。
 *   card_index - 卡片在 order_cards 数组中的下标，也是初始占位图片对应的菜品下标。
 *   x          - 卡片相对于 parent 的横坐标。
 *   y          - 卡片相对于 parent 的纵坐标。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
static void create_dish_card(lv_obj_t *parent, int card_index, int x, int y)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_t *remove_button;
    lv_obj_t *add_button;
    lv_obj_t *button_label;
    order_card_t *card_data = &order_cards[card_index];

    card_data->root = card;
    card_data->dish_index = -1;
    lv_obj_set_size(card, 188, 292);
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    clear_layout_padding(card);
    lv_obj_set_style_radius(card, 16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFDF6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFE7C8), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 14, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xAC6C33), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, LV_PART_MAIN);

    create_food_placeholder(card, &dishes[card_index]);

    /* A real food image is shown when this dish has one; otherwise the
     * coloured placeholder behind it remains visible. */
    card_data->food_image = lv_img_create(card);
    lv_obj_set_pos(card_data->food_image, 14, 13);
    if(dishes[card_index].image != NULL) {
        lv_img_set_src(card_data->food_image, dishes[card_index].image);
    } else {
        lv_obj_add_flag(card_data->food_image, LV_OBJ_FLAG_HIDDEN);
    }

    card_data->name_label = create_label(card, "菜品", lv_color_hex(0x4B2F20), 14, 131);
    lv_obj_set_style_text_font(card_data->name_label, &lv_font_source_han_sans_normal_16, 0);
    /* 固定名称区域宽度，避免中文菜名在卡片中被默认尺寸裁剪。 */
    lv_obj_set_size(card_data->name_label, 160, 26);
    lv_label_set_long_mode(card_data->name_label, LV_LABEL_LONG_CLIP);
    card_data->price_label = create_label(card, "￥0.00", lv_color_hex(0xEE531C), 14, 160);
    create_label(card, "点击加号加入购物车", lv_color_hex(0x8F6F55), 14, 188);
    /* 减号按钮回执事件 */
    remove_button = lv_btn_create(card);
    lv_obj_set_size(remove_button, 46, 46);
    lv_obj_set_pos(remove_button, 14, 228);
    lv_obj_set_style_radius(remove_button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(remove_button, lv_color_hex(0xFFF8EF), LV_PART_MAIN);
    lv_obj_set_style_border_color(remove_button, lv_color_hex(0xF5B070), LV_PART_MAIN);
    lv_obj_set_style_border_width(remove_button, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(remove_button, 0, LV_PART_MAIN);
    button_label = lv_label_create(remove_button);
    lv_label_set_text(button_label, "-");
    lv_obj_set_style_text_font(button_label, &ui_font_test1, 0);
    lv_obj_set_style_text_color(button_label, lv_color_hex(0xE55D21), 0);
    lv_obj_center(button_label);
    lv_obj_add_event_cb(remove_button, remove_button_event_cb, LV_EVENT_CLICKED, card_data);
    
    card_data->quantity_label = lv_label_create(card);
    lv_label_set_text(card_data->quantity_label, "0");
    lv_obj_set_style_text_font(card_data->quantity_label, &ui_font_test1, 0);
    lv_obj_set_style_text_color(card_data->quantity_label, lv_color_hex(0x573925), 0);
    lv_obj_set_width(card_data->quantity_label, 36);
    lv_obj_set_pos(card_data->quantity_label, 76, 241);
    lv_obj_set_style_text_align(card_data->quantity_label, LV_TEXT_ALIGN_CENTER, 0);
    /* 加号按钮回执事件 */
    add_button = lv_btn_create(card);
    lv_obj_set_size(add_button, 46, 46);
    lv_obj_set_pos(add_button, 128, 228);
    lv_obj_set_style_radius(add_button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(add_button, lv_color_hex(0xF45B19), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(add_button, lv_color_hex(0xFF9837), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(add_button, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_width(add_button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(add_button, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(add_button, lv_color_hex(0xEB5B1F), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(add_button, LV_OPA_50, LV_PART_MAIN);
    button_label = lv_label_create(add_button);
    lv_label_set_text(button_label, "+");
    lv_obj_set_style_text_font(button_label, &ui_font_test1, 0);
    lv_obj_set_style_text_color(button_label, lv_color_white(), 0);
    lv_obj_center(button_label);
    lv_obj_add_event_cb(add_button, add_button_event_cb, LV_EVENT_CLICKED, card_data); 
}

/**
 * 函数功能：创建完整的点餐屏幕。
 *
 * 创建顶部状态栏、左侧分类按钮、可滚动菜品卡片区、底部购物车栏和购物车屏幕，
 * 最后调用 refresh_order_ui() 显示初始菜品数据。
 *
 * 参数：
 *   screen - SquareLine 创建的点餐屏幕对象 ui_Screen1，所有点餐控件均添加到该对象。
 *
 * 返回值：
 *   无。该函数的返回类型为 void。
 */
void ui_order_screen_create(lv_obj_t *screen)
{
    static int category_ids[ORDER_CATEGORY_COUNT] = {
        ORDER_CATEGORY_ALL, ORDER_CATEGORY_MAIN, ORDER_CATEGORY_SNACK, ORDER_CATEGORY_DRINK, ORDER_CATEGORY_DESSERT
    };
    lv_obj_t *header;
    lv_obj_t *cart_panel;
    lv_obj_t *cart_image;
    lv_obj_t *checkout_button;
    int i;

    order_screen = screen;

    header = lv_obj_create(screen);
    lv_obj_set_size(header, 800, 59);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    clear_layout_padding(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x441E12), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);

    create_label(header, "云嵌食堂", lv_color_white(), 20, 20);
    create_label(header, "点餐", lv_color_white(), 382, 20);
    status_label = create_label(header, "已连接", lv_color_hex(0xFFEEC3), 704, 20);

    for(i = 0; i < ORDER_CATEGORY_COUNT; i++) {
        category_buttons[i] = lv_btn_create(screen);
        lv_obj_set_size(category_buttons[i], 100, 45);
        lv_obj_set_pos(category_buttons[i], 20, 83 + i * 55);
        lv_obj_set_style_radius(category_buttons[i], 11, LV_PART_MAIN);
        lv_obj_set_style_border_width(category_buttons[i], 1, LV_PART_MAIN);
        lv_obj_set_style_text_font(category_buttons[i], &lv_font_source_han_sans_normal_16, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(category_buttons[i], 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_x(category_buttons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_text_align(category_buttons[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_t *label = lv_label_create(category_buttons[i]);
        lv_label_set_text(label, category_names[i]);
        style_chinese_label(label, lv_color_hex(0x5B3A27));
        lv_obj_center(label);
        lv_obj_add_event_cb(category_buttons[i], category_button_event_cb, LV_EVENT_CLICKED, &category_ids[i]);
    }

    /* 菜品列表可上下滑动。三列放满后，后续菜品自动排到下一行。 */
    dish_list = lv_obj_create(screen);
    lv_obj_set_size(dish_list, 658, 315);
    lv_obj_set_pos(dish_list, 142, 78);
    lv_obj_set_scroll_dir(dish_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(dish_list, LV_SCROLLBAR_MODE_AUTO);
    clear_layout_padding(dish_list);
    lv_obj_set_style_bg_opa(dish_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dish_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dish_list, 0, LV_PART_MAIN);

    for(i = 0; i < ORDER_CARD_COUNT; i++) {
        int column = i % 3;
        int row = i / 3;
        create_dish_card(dish_list, i, column * 200, row * 302);
    }

    cart_panel = lv_obj_create(screen);
    lv_obj_set_size(cart_panel, 640, 68);
    lv_obj_set_pos(cart_panel, 142, 400);
    lv_obj_clear_flag(cart_panel, LV_OBJ_FLAG_SCROLLABLE);
    clear_layout_padding(cart_panel);
    lv_obj_set_style_radius(cart_panel, 15, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cart_panel, lv_color_hex(0xFFFDF7), LV_PART_MAIN);
    lv_obj_set_style_border_color(cart_panel, lv_color_hex(0xFFE6C0), LV_PART_MAIN);
    lv_obj_set_style_border_width(cart_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(cart_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(cart_panel, lv_color_hex(0xB0703E), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(cart_panel, LV_OPA_20, LV_PART_MAIN);

    cart_image = lv_img_create(cart_panel);
    lv_img_set_src(cart_image, &ui_img_order_cart);
    lv_obj_set_pos(cart_image, 15, 13);
    create_label(cart_panel, "购物车", lv_color_hex(0x523220), 66, 13);
    cart_count_label = create_label(cart_panel, "已选 0 份菜品", lv_color_hex(0x876245), 66, 36);
    cart_total_label = create_label(cart_panel, "合计 ￥0.00", lv_color_hex(0xE04C18), 372, 26);

    /* 点击购物车区域进入购物车屏幕。子控件把点击事件传递给父容器。 */
    lv_obj_add_flag(cart_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cart_panel, cart_panel_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(cart_image, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(cart_count_label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(cart_total_label, LV_OBJ_FLAG_EVENT_BUBBLE);

    checkout_button = lv_btn_create(cart_panel);
    lv_obj_set_size(checkout_button, 160, 55);
    lv_obj_set_pos(checkout_button, 470, 6);
    lv_obj_set_style_bg_img_src(checkout_button, &ui_img_order_checkout, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(checkout_button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(checkout_button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(checkout_button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(checkout_button, checkout_button_event_cb, LV_EVENT_CLICKED, NULL);

    create_cart_screen();
    refresh_order_ui();
}

/**
 * 函数功能：连接阿里云服务器并加载最新菜单。
 *
 * 程序启动时由 main.c 调用；网络断开后，ensure_server_connected() 也会调用本函数尝试重连。
 * 连接成功后发送 MENU 命令，用服务器返回的名称、分类和价格更新本地显示数据。
 *
 * 参数：
 *   无。
 *
 * 返回值：
 *   无。连接成功或失败通过右上角 status_label 的文字提示给用户。
 */
void ui_order_connect_server(void)
{
    order_menu_item_t menu[ORDER_CLIENT_MENU_MAX];
    size_t menu_count;

    if(status_label == NULL) {
        return;
    }

    lv_label_set_text(status_label, "连接中");
    order_client_close(&order_client);

    if(order_client_connect(&order_client, ORDER_SERVER_IP, ORDER_SERVER_PORT) != 0) {
        lv_label_set_text(status_label, "未连接");
        return;
    }

    if(order_client_load_menu(&order_client, menu, ORDER_CLIENT_MENU_MAX, &menu_count) != 0) {
        order_client_close(&order_client);
        lv_label_set_text(status_label, "菜单失败");
        return;
    }
    // 入口
    apply_server_menu(menu, menu_count);  
    clear_local_cart();
    lv_label_set_text(status_label, "已连接");
    refresh_order_ui();
}
