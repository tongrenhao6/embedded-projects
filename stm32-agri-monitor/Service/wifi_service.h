/* wifi_service.h - ESP8266 AT + TCP transparent mode + JSON upload */
#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <stdint.h>

void wifi_service_init(void);
int  wifi_service_connect(void);            /* STA / CWJAP / CIPMODE+CIPSTART(OneNET) / MQTT CONNECT */
int  wifi_service_enter_transparent(void);  /* 透传已在 connect 内完成（保留接口） */
int  wifi_service_send(const char *json);   /* 透传发送文本 */
int  wifi_service_mqtt_publish(const char *topic, const char *payload);  /* PUBLISH(QoS0) */
void wifi_service_mqtt_poll_rx(void);        /* 解析 CONNACK / SUBACK / reply / 属性设置 PUBLISH */
int  wifi_service_mqtt_flush_set_reply(void);/* 发送待回复的属性设置结果 */
void wifi_service_close_tcp(void);           /* 断开当前 TCP，便于重连 */
void wifi_service_set_ctrl_cb(void (*cb)(uint8_t beep_on));  /* 收到 beep_ctrl 设置时回调：1=允许响 0=静音 */
int  wifi_service_mqtt_keepalive(void);      /* 发送 MQTT PINGREQ 保活，防平台超时断开 */
int  wifi_service_reset_module(void);        /* 连续重连失败时软件复位 ESP8266 */
int  wifi_service_is_ready(void);

#endif
