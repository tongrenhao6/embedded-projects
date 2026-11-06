/* app_config.h - 应用级配置：单工程双固件宏切换、串口波特率等 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* =========================================================================
 * 固件角色切换：
 *   ROLE_GATEWAY = 1  -> 编译网关固件（汇聚 + ESP8266 TCP/JSON 上传）
 *   ROLE_GATEWAY = 0  -> 编译终端固件（采集 + 本地报警 + LoRa 上报）
 * 通过修改本宏（或在 Keil 编译宏中定义 ROLE_GATEWAY=1/0）切换。
 * ========================================================================= */
#ifndef ROLE_GATEWAY
#define ROLE_GATEWAY       0U
#endif

/* 调试串口（USART1）—— 系统自定义，统一 9600 */
#define USART1_BAUDRATE    9600U

/* 数据串口（USART2）：
 *   终端:CO2 传感器（设备默认 9600）
 *   网关:ESP8266（设备默认波特率，一般 115200，请按模块实测调整） */
#if (ROLE_GATEWAY == 1U)
#define USART2_BAUDRATE    115200U
#define USART2_ROLE        "ESP8266"
#else
#define USART2_BAUDRATE    9600U
#define USART2_ROLE        "CO2"
#endif

/* 分布式节点规模 */
#define MAX_NODE_NUM       8U            /* 网关最多管理的终端节点数 */
#define DEFAULT_NODE_COUNT 1U            /* 默认（网关轮询 1..N） 当前只有一个终端设备*/
#define NODE_ID_GATEWAY    0x01U         /* 网关 LoRa 节点地址 */
#define NODE_ID_BASE       0x02U         /* 终端节点起始地址 */

/* 默认采集/上报周期（ms），可通过参数区修改 */
#define DEFAULT_COLLECT_MS 1000U		//网关向终端轮询数据的周期
#define DEFAULT_UPLOAD_MS  3000U		//网关数据上报到onenet的周期

/* 默认报警阈值（终端本地判断用，可参数下发修改） */
#define DEFAULT_TEMP_HIGH  30.0f
#define DEFAULT_TEMP_LOW   10.0f
#define DEFAULT_HUMI_HIGH  80.0f
#define DEFAULT_HUMI_LOW   20.0f
#define DEFAULT_CO2_HIGH   1000U
#define DEFAULT_LIGHT_LOW  50U
#define DEFAULT_SOIL_LOW   20U     /* 现在：湿度 <20% 报干燥 */

/* 默认 WIFI 连接参数（网关，存参数区） */
#define DEFAULT_WIFI_SSID    "YOUR_WIFI_SSID"
#define DEFAULT_WIFI_PSWD    "YOUR_WIFI_PASSWORD"
#define DEFAULT_SERVER_IP    "127.0.0.1"
#define DEFAULT_SERVER_PORT  8000U

/* 任务优先级 */
#define TASK_PRIO_SENSOR    3U
#define TASK_PRIO_LORA      4U
#define TASK_PRIO_WIFI      3U
#define TASK_PRIO_OLED      1U
#define TASK_PRIO_ALARM     2U
#define TASK_PRIO_MAIN      2U

/* ===== OneNET MQTT 连接常量（OneNET Studio 属性上报） ===== */
#define ONENET_IP        "YOUR_PRODUCT_ID.mqtts.acc.cmcconenet.cn"
#define ONENET_PORT      1883U
#define ONENET_CLIENTID  "YOUR_DEVICE_NAME"
#define ONENET_USERNAME  "YOUR_PRODUCT_ID"
#define ONENET_PASSWD    "YOUR_DEVICE_TOKEN"
#define ONENET_TOPIC_UP    "$sys/YOUR_PRODUCT_ID/YOUR_DEVICE_NAME/thing/property/post"
#define ONENET_TOPIC_REPLY "$sys/YOUR_PRODUCT_ID/YOUR_DEVICE_NAME/thing/property/post/reply"
#define ONENET_TOPIC_SET   "$sys/YOUR_PRODUCT_ID/YOUR_DEVICE_NAME/thing/property/set"
#define ONENET_TOPIC_SET_REPLY "$sys/YOUR_PRODUCT_ID/YOUR_DEVICE_NAME/thing/property/set_reply"

/* 蜂鸣器静音自动恢复时长（ms）：手动关闭(beep_ctrl=false)后 60s 自动恢复允许响 */
#define BEEP_MUTE_AUTO_RESET_MS 60000U

#endif /* APP_CONFIG_H */
