#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cart.h"
#include "order_service.h"

void test_menu_loads_dishes_from_file(void);

void test_cart_accumulates_quantity_and_total(void)
{
    dish_t rice = { .id = 1, .category = "主食", .name = "鸡腿饭", .price = 12.0f };
    dish_t tea = { .id = 2, .category = "饮品", .name = "柠檬茶", .price = 5.0f };
    cart_t cart;

    cart_init(&cart);
    assert(cart_add(&cart, &rice, 2) == 0);
    assert(cart_add(&cart, &tea, 1) == 0);
    assert(cart_add(&cart, &rice, 1) == 0);
    assert(cart_total_price(&cart) == 41.0f);
    assert(cart_remove(&cart, 1) == 0);
    assert(cart_total_price(&cart) == 29.0f);
    cart_clear(&cart);
}

void test_service_adds_dishes_and_writes_an_order(void)
{
    order_service_t service;
    char response[128];
    FILE *orders;
    char order_line[128];

    order_service_reset_order_number();
    remove("tests/fixtures/orders_test.txt");
    assert(order_service_init(&service, "tests/fixtures/menu.txt", "tests/fixtures/orders_test.txt") == 0);
    assert(order_service_handle(&service, "ADD|1", response, sizeof(response)) == 0);
    assert(order_service_handle(&service, "ADD|1", response, sizeof(response)) == 0);
    assert(strcmp(response, "OK|CART|24.00\n") == 0);
    assert(order_service_handle(&service, "CART", response, sizeof(response)) == 0);
    assert(strcmp(response, "OK|CART|24.00\n") == 0);
    assert(order_service_handle(&service, "SUBMIT", response, sizeof(response)) == 0);
    assert(strcmp(response, "OK|ORDER|1|24.00\n") == 0);

    orders = fopen("tests/fixtures/orders_test.txt", "r");
    assert(orders != NULL);
    assert(fgets(order_line, sizeof(order_line), orders) != NULL);
    assert(strstr(order_line, "1|24.00|1,2") != NULL);
    fclose(orders);
    order_service_destroy(&service);
}

void test_two_clients_use_different_order_numbers(void)
{
    order_service_t first_client;
    order_service_t second_client;
    char response[128];

    order_service_reset_order_number();
    remove("tests/fixtures/orders_first_client.txt");
    remove("tests/fixtures/orders_second_client.txt");
    assert(order_service_init(&first_client, "tests/fixtures/menu.txt", "tests/fixtures/orders_first_client.txt") == 0);
    assert(order_service_init(&second_client, "tests/fixtures/menu.txt", "tests/fixtures/orders_second_client.txt") == 0);

    assert(order_service_handle(&first_client, "ADD|1", response, sizeof(response)) == 0);
    assert(order_service_handle(&first_client, "SUBMIT", response, sizeof(response)) == 0);
    assert(strcmp(response, "OK|ORDER|1|12.00\n") == 0);

    assert(order_service_handle(&second_client, "ADD|2", response, sizeof(response)) == 0);
    assert(order_service_handle(&second_client, "SUBMIT", response, sizeof(response)) == 0);
    assert(strcmp(response, "OK|ORDER|2|5.00\n") == 0);

    order_service_destroy(&first_client);
    order_service_destroy(&second_client);
}

int main(void)
{
    test_menu_loads_dishes_from_file();
    test_cart_accumulates_quantity_and_total();
    test_service_adds_dishes_and_writes_an_order();
    test_two_clients_use_different_order_numbers();
    puts("server core tests passed");
    return 0;
}
