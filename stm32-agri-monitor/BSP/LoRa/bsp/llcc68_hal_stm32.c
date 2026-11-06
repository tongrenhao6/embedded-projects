#include "llcc68_hal.h"
#include "llcc68_stm32_port.h"

/** 等待 BUSY 拉低，并用超时防止硬件异常导致程序永久阻塞。 */
static llcc68_hal_status_t llcc68_wait_on_busy(void)
{
    const uint32_t started_at = HAL_GetTick();

    while (HAL_GPIO_ReadPin(LLCC68_BUSY_GPIO_Port, LLCC68_BUSY_Pin) == GPIO_PIN_SET)
    {
        if ((HAL_GetTick() - started_at) >= LLCC68_BUSY_TIMEOUT_MS)
        {
            return LLCC68_HAL_STATUS_ERROR;
        }
    }
    return LLCC68_HAL_STATUS_OK;
}

/** 在同一次 NSS 低电平期间连续发送命令和可选数据。 */
llcc68_hal_status_t llcc68_hal_write(const void* context, const uint8_t* command,
                                     const uint16_t command_length, const uint8_t* data,
                                     const uint16_t data_length)
{
    /* 单模块方案不需要使用上下文参数。 */
    (void)context;
    if (llcc68_wait_on_busy() != LLCC68_HAL_STATUS_OK)
        return LLCC68_HAL_STATUS_ERROR;

    /* NSS 拉低后开始一次完整的 SPI 命令事务。 */
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)command, command_length,
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
        goto error;

    if (data_length)
        if(HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)data, data_length,
                          LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
            goto error;

    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return llcc68_wait_on_busy();

error:
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return LLCC68_HAL_STATUS_ERROR;
}

/** 发送读命令，并在 NSS 保持低电平时接收返回数据。 */
llcc68_hal_status_t llcc68_hal_read(const void* context, const uint8_t* command,
                                    const uint16_t command_length, uint8_t* data,
                                    const uint16_t data_length)
{
    (void)context;
    if (llcc68_wait_on_busy() != LLCC68_HAL_STATUS_OK)
        return LLCC68_HAL_STATUS_ERROR;

    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)command, command_length,
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
        goto error;

    /* STM32 在主机接收模式下通过 MOSI 发送占位字节以产生时钟。 */
    if ((data_length != 0U) &&
        (HAL_SPI_Receive(&LLCC68_SPI_HANDLE, data, data_length,
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK))
        goto error;

    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return llcc68_wait_on_busy();

error:
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return LLCC68_HAL_STATUS_ERROR;
}

/** 产生低电平复位脉冲，然后等待芯片重新就绪。 */
llcc68_hal_status_t llcc68_hal_reset(const void* context)
{
    (void)context;
    HAL_GPIO_WritePin(LLCC68_RESET_GPIO_Port, LLCC68_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(LLCC68_RESET_GPIO_Port, LLCC68_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    return llcc68_wait_on_busy();
}

/** 通过 NSS 下降沿和 GetStatus 命令将芯片从休眠状态唤醒。 */
llcc68_hal_status_t llcc68_hal_wakeup(const void* context)
{
    const uint8_t get_status[2] = { 0xC0U, LLCC68_NOP };
    (void)context;

    /* NSS 下降沿唤醒射频芯片，随后发送 GetStatus 完成唤醒。 */
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)get_status, sizeof(get_status),
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
    {
        HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
        return LLCC68_HAL_STATUS_ERROR;
    }
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return llcc68_wait_on_busy();
}
