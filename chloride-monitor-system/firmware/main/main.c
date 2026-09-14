
 
#include "nvs_flash.h"
#include "iic.h"
#include "xl9555.h"
#include "lvgl_demo.h"
#include "spi.h"
#include "key.h"
#include "led.h"
#include "24cxx.h"
#include "manage.h"
#include "adc1.h"
#include "ap3216c.h"
#include "qma6100p.h"
#include "exfuns.h"
#include "esp_rtc.h"
#include "ph_time.h"
#include "ph_wifi.h"
#include "ph_mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* I2C主设备对象 */
i2c_obj_t i2c0_master;

static const char *TAG = "app_main";

static void network_init_task(void *arg)
{
    esp_err_t wifi_ret;
    uint32_t wait_count = 0;

    (void)arg;
    ESP_LOGI(TAG, "init network");
    wifi_ret = ph_wifi_init();
    if (wifi_ret == ESP_OK)
    {
        while (!ph_wifi_is_connected())
        {
            if ((wait_count % 10U) == 0U)
            {
                ESP_LOGI(TAG, "waiting for wifi ip, status=%s", ph_wifi_get_status());
            }
            wait_count++;
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        ESP_LOGI(TAG, "wifi ready, status=%s", ph_wifi_get_status());
        ESP_LOGI(TAG, "init sntp");
        ph_time_init();
        ESP_LOGI(TAG, "start mqtt client");
        ph_mqtt_client_start();
    }
    else
    {
        ESP_LOGW(TAG, "network init returned %s", esp_err_to_name(wifi_ret));
    }

    vTaskDelete(NULL);
}
 
/* 函数声明 */
void app_main(void)
{
    esp_err_t ret;
     
    /* 初始化NVS存储 */
    ret = nvs_flash_init();             
 
    /* 如果NVS初始化失败，尝试擦除并重新初始化 */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
 
    /* 检查NVS初始化结果 */
    ESP_ERROR_CHECK(ret);
    /* 初始化I2C0 */
    ESP_LOGI(TAG, "init i2c0");
    i2c0_master = iic_init(I2C_NUM_0);  
    /* 初始化SPI2 */
    ESP_LOGI(TAG, "init spi2");
    spi2_init();                        
    /* 初始化LED */
    ESP_LOGI(TAG, "init led");
    led_init();                         
    /* 初始化按键 */
    ESP_LOGI(TAG, "init key");
    key_init();                         
    /* 初始化XL9555 */
    ESP_LOGI(TAG, "init xl9555");
    xl9555_init(i2c0_master);           
    /* 初始化24CXX */
    ESP_LOGI(TAG, "init at24cxx");
    at24cxx_init(i2c0_master);          
    /* 初始化AP3216C */
    ESP_LOGI(TAG, "init ap3216c");
    ap3216c_init(i2c0_master);          
    /* 初始化QMA6100P */
    ESP_LOGI(TAG, "init qma6100p");
    qma6100p_init(i2c0_master);         
    /* 初始化LCD */
    ESP_LOGI(TAG, "init lcd");
    lcd_init();                         
    /* 初始化扩展函数 */
    ESP_LOGI(TAG, "init exfuns");
    exfuns_init();                      

    /* 网络放到后台初始化，避免Wi-Fi连接过程挡住菜单启动。 */
    if (xTaskCreate(network_init_task, "net_init", 6144, NULL, 4, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "create network task failed");
    }

    /* 启动LVGL示例 */
    ESP_LOGI(TAG, "start lvgl demo");
    lvgl_demo();                        
}
