/*
    该文件用于配置wifi 连接配置
*/

#ifndef __PH_CONFIG_H
#define __PH_CONFIG_H

/* Fill these two fields before building if fixed Wi-Fi auto-connect is needed. */
#define PH_WIFI_SSID       "sensortest"  //Wi-Fi 名称
#define PH_WIFI_PASSWORD   "333444555"  //Wi-Fi 密码

#define PH_SNTP_SERVER_1   "ntp.aliyun.com"
#define PH_SNTP_SERVER_2   "cn.pool.ntp.org"
#define PH_SNTP_SERVER_3   "pool.ntp.org"

#define PH_DEVICE_ID       "ph_terminal_001"

#define PH_MQTT_BROKER_URI "mqtt://120.27.235.176:1883"
#define PH_MQTT_USERNAME   "root"
#define PH_MQTT_PASSWORD   "123"
#define PH_MQTT_CLIENT_ID  PH_DEVICE_ID
#define PH_MQTT_BASE_TOPIC "phwater/v1"

/*
 * RS485 UART pins.
 * GPIO16/GPIO17 are also camera D5/D6 on the DNESP32-S3 BSP, so move these
 * to another exposed header pair if the camera connector or board wiring
 * interferes with UART1.
 */
#define PH_RS485_UART_TX_GPIO  16
#define PH_RS485_UART_RX_GPIO  17

#endif
