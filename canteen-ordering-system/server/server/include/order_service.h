#ifndef ORDER_SERVICE_H
#define ORDER_SERVICE_H

#include <stddef.h>

#include "cart.h"
#include "menu.h"

typedef struct {
    menu_t menu;
    cart_t cart;
    char orders_path[256];
} order_service_t;

/* 服务端启动时调用一次，使订单号从 1 开始。 */
void order_service_reset_order_number(void);
int order_service_init(order_service_t *service, const char *menu_path, const char *orders_path);
int order_service_handle(order_service_t *service, const char *request, char *response, size_t response_size);
void order_service_destroy(order_service_t *service);

#endif
