/* freertos.c - Thread framework (CubeMX style, CMSIS-RTOS2) */

/* 1. 必须最先包含 FreeRTOS.h */
#include "FreeRTOS.h"

/* 2. 然后才是其它 FreeRTOS 头 */
#include "task.h"
#include "queue.h"

/* 3. CMSIS-RTOS2 封装 */
#include "cmsis_os2.h"

/* 4. 最后才是工程自己的头 */
#include "main.h"
#include "app_freertos.h"

/* ---- 任务入口：实现在 App 层 ---- */
extern void NodeSensorTask(void *argument);
extern void NodeLoraTask(void *argument);
extern void NodeAlarmTask(void *argument);
extern void NodeDisplayTask(void *argument);
extern void GatewayLoraTask(void *argument);
extern void GatewayWifiTask(void *argument);
extern void GatewayDisplayTask(void *argument);

static const osThreadAttr_t sensor_attr = {
  .name = "SensorTask", .attr_bits = 0,
  .cb_mem = NULL, .cb_size = 0,
  .stack_mem = NULL, .stack_size = 1280,
  .priority = osPriorityAboveNormal4, .tz_module = 0, .reserved = 0
};
static const osThreadAttr_t lora_attr = {
  .name = "LoraTask", .attr_bits = 0,
  .cb_mem = NULL, .cb_size = 0,
  .stack_mem = NULL, .stack_size = 1024,
  .priority = osPriorityHigh, .tz_module = 0, .reserved = 0
};
static const osThreadAttr_t wifi_attr = {
  .name = "WifiTask", .attr_bits = 0,
  .cb_mem = NULL, .cb_size = 0,
  .stack_mem = NULL, .stack_size = 2048,
  .priority = osPriorityAboveNormal4, .tz_module = 0, .reserved = 0
};
static const osThreadAttr_t alarm_attr = {
  .name = "AlarmTask", .attr_bits = 0,
  .cb_mem = NULL, .cb_size = 0,
  .stack_mem = NULL, .stack_size = 512,
  .priority = osPriorityAboveNormal2, .tz_module = 0, .reserved = 0
};
static const osThreadAttr_t display_attr = {
  .name = "DisplayTask", .attr_bits = 0,
  .cb_mem = NULL, .cb_size = 0,
  .stack_mem = NULL, .stack_size = 512,
  .priority = osPriorityLow2, .tz_module = 0, .reserved = 0
};

void MX_FREERTOS_Init(void)
{
  osKernelInitialize();

#if (ROLE_GATEWAY == 1U)
  osThreadNew(GatewayLoraTask, NULL, &lora_attr);
  osThreadNew(GatewayWifiTask, NULL, &wifi_attr);
  osThreadNew(GatewayDisplayTask, NULL, &display_attr);
#else
  osThreadNew(NodeSensorTask, NULL, &sensor_attr);
  osThreadNew(NodeLoraTask, NULL, &lora_attr);
  osThreadNew(NodeAlarmTask, NULL, &alarm_attr);
  osThreadNew(NodeDisplayTask, NULL, &display_attr);
#endif
}
