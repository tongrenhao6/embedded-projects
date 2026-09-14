#include "board_info.h"

#include <inttypes.h>

#include "app_config.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

static const char *TAG = "board_info";

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "power-on";
    case ESP_RST_EXT:
        return "external";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt watchdog";
    case ESP_RST_TASK_WDT:
        return "task watchdog";
    case ESP_RST_WDT:
        return "other watchdog";
    case ESP_RST_DEEPSLEEP:
        return "deep sleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "sdio";
    default:
        return "unknown";
    }
}

void board_info_log(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;

    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "Target: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "CPU cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "Silicon revision: v%d.%d",
             chip_info.revision / 100,
             chip_info.revision % 100);
    ESP_LOGI(TAG, "Reset reason: %s", reset_reason_name(esp_reset_reason()));

    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "Flash: %" PRIu32 " MB", flash_size / (1024U * 1024U));
    } else {
        ESP_LOGW(TAG, "Flash size read failed");
    }

    ESP_LOGI(TAG, "Heap free/minimum: %" PRIu32 "/%" PRIu32 " bytes",
             esp_get_free_heap_size(),
             esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "PSRAM total/free: %u/%u bytes",
             (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Reserved GPIOs: BOOT=%d USB_DM=%d USB_DP=%d RGB=%d",
             APP_RESERVED_BOOT_GPIO,
             APP_RESERVED_USB_DM_GPIO,
             APP_RESERVED_USB_DP_GPIO,
             APP_RESERVED_RGB_GPIO);
}
