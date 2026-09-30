SET(SOURCES screens/ui_shouye.c
    screens/ui_Screen1.c
    ui.c
    ui_order.c
    order_client.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_926244553.c
    images/ui_img_113425367.c
    images/ui_img_585191297.c
    images/ui_img_order_cart.c
    images/ui_img_order_checkout.c
    images/ui_img_food_chicken_rice.c
    images/ui_img_food_beef_noodles.c
    images/ui_img_food_pork_feet_rice.c
    images/ui_img_food_fried_chicken.c
    images/ui_img_food_french_fries.c
    images/ui_img_food_grilled_skewers.c
    images/ui_img_food_mineral_water.c
    images/ui_img_food_lemon_tea.c
    images/ui_img_food_hot_milk.c
    images/ui_img_food_cream_puff.c
    images/ui_img_food_pudding.c
    images/ui_img_food_cake.c
    fonts/ui_font_test1.c)

add_library(ui ${SOURCES})
