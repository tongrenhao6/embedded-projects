/* gateway_app.h - gateway firmware high-level app (gateway role) */
#ifndef GATEWAY_APP_H
#define GATEWAY_APP_H

void gateway_app_init(void);

/* FreeRTOS task entries (referenced by freertos.c) */
void GatewayLoraTask(void *argument);
void GatewayWifiTask(void *argument);
void GatewayDisplayTask(void *argument);

#endif
