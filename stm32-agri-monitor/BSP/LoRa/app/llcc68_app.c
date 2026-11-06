#include "llcc68_app.h"
#include "llcc68.h"
#include "llcc68_hal.h"
#include "stddef.h"
#include "stdio.h"

#define LLCC68_RF_FREQUENCY_HZ  470000000UL
#define LLCC68_TX_POWER_DBM     14
#define LLCC68_LORA_SYNC_WORD   0x12U
#define LLCC68_RX_MAX_LENGTH    255U

/* LoRa 数据包参数。发送前必须把 pld_len_in_bytes 改成实际发送长度。 */
static llcc68_pkt_params_lora_t lora_packet = {
    .preamble_len_in_symb = 8,
    .header_type = LLCC68_LORA_PKT_EXPLICIT,
    .pld_len_in_bytes = LLCC68_RX_MAX_LENGTH,
    .crc_is_on = true,
    .invert_iq_is_on = false,
};

/* 上层注册的数据接收回调；未注册时保持为空。 */
static llcc68_app_rx_callback_t rx_callback;

/** 保存上层接收回调。 */
void llcc68_app_set_rx_callback(llcc68_app_rx_callback_t callback)
{
    rx_callback = callback;
}

/**
 * 复位芯片并配置本项目使用的 LoRa 参数。
 * 主机与从机的频率、SF、带宽、编码率及数据包参数必须一致。
 */
void llcc68_app_init(void)
{
    /* LoRa 调制方式：SF7、125 kHz 带宽、4/5 编码率。 */
    const llcc68_mod_params_lora_t modulation = {
        .sf = LLCC68_LORA_SF5, .bw = LLCC68_LORA_BW_125,
        .cr = LLCC68_LORA_CR_4_5, .ldro = 0,
    };
    /* LLCC68 高功率 PA 参数；实际发射功率仍由 SetTxParams 的 14 dBm 决定。 */
    const llcc68_pa_cfg_params_t pa_config = {
        .pa_duty_cycle = 0x04,
        .hp_max = 0x07,
        .device_sel = 0x00,
        .pa_lut = 0x01,
    };

    /* 复位设备 */
    if(llcc68_reset(NULL)!=LLCC68_HAL_STATUS_OK)
			printf("LLCC68 reset failed\r\n");
		
		/* 待机状态 */
    llcc68_set_standby(NULL, LLCC68_STANDBY_CFG_RC);

    /* 使用 DIO2 自动切换模块内部的收发射频开关。 */
    llcc68_set_dio2_as_rf_sw_ctrl(NULL, true);
		/* Ra-01SC 使用 LDO 供电模式；复位后仍显式设置，避免依赖默认值。 */
		llcc68_set_reg_mode(NULL, LLCC68_REG_MODE_LDO);
		
		/* 按官方初始化顺序校准 RC、PLL、ADC 和镜像抑制电路。 */
		llcc68_cal(NULL, LLCC68_CAL_ALL);
    llcc68_set_pa_cfg(NULL, &pa_config);
    llcc68_cfg_tx_clamp(NULL);

    llcc68_set_pkt_type(NULL, LLCC68_PKT_TYPE_LORA);
    /* 对 470 MHz 工作频段执行镜像校准。 */
    llcc68_cal_img_in_mhz(NULL, 470U, 490U);
    llcc68_set_rf_freq(NULL, LLCC68_RF_FREQUENCY_HZ);
    llcc68_set_lora_mod_params(NULL, &modulation);
    llcc68_set_lora_pkt_params(NULL, &lora_packet);
    /* 显式设置私有网络同步字，避免依赖芯片上电默认值。 */
    llcc68_set_lora_sync_word(NULL, LLCC68_LORA_SYNC_WORD);
    llcc68_set_buffer_base_address(NULL, 0x00, 0x00);
    llcc68_set_tx_params(NULL, LLCC68_TX_POWER_DBM, LLCC68_RAMP_200_US);
    /* 把收发完成、超时和 CRC 错误中断全部映射到 DIO1。 */
    llcc68_set_dio_irq_params(NULL,
        LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_TIMEOUT | LLCC68_IRQ_CRC_ERROR,
        LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_TIMEOUT | LLCC68_IRQ_CRC_ERROR,
        LLCC68_IRQ_NONE, LLCC68_IRQ_NONE);
    llcc68_clear_irq_status(NULL, LLCC68_IRQ_ALL);
}

/** 清除遗留中断并进入连续接收模式。 */
void llcc68_app_start_receive(void)
{
	llcc68_chip_status_t radio_status;
	static uint8_t first_rx_check = 1U;

    /* 显式包头接收时允许接收最长 255 字节的数据。 */
    lora_packet.pld_len_in_bytes = LLCC68_RX_MAX_LENGTH;
    llcc68_set_lora_pkt_params(NULL, &lora_packet);
    llcc68_clear_irq_status(NULL, LLCC68_IRQ_ALL);
    /*
     * LLCC68_RX_CONTINUOUS(0xFFFFFF)是芯片RTC步数，不是毫秒值。
     * 必须调用rtc_step版本；传给llcc68_set_rx()会因超过毫秒上限而被驱动拒绝，
     * 芯片将一直停留在Standby RC模式。
     */
    if (//llcc68_set_rx( NULL,LLCC68_RX_CONTINUOUS)
			llcc68_set_rx_with_timeout_in_rtc_step(NULL, LLCC68_RX_CONTINUOUS) != LLCC68_STATUS_OK)
    {
        printf("ERROR: SetRx continuous failed\r\n");
    }

	/* 首次进入接收后读取芯片模式，确认 SPI 命令确实生效。 */
	if (first_rx_check != 0U)
	{
		first_rx_check = 0U;
		if (llcc68_get_status(NULL, &radio_status) != LLCC68_STATUS_OK)
		{
			printf("LoRa status read failed\r\n");
		}
		else
		{
			printf("LoRa mode=%u, cmd=%u\r\n",
				   (unsigned int)radio_status.chip_mode,
				   (unsigned int)radio_status.cmd_status);
			if (radio_status.chip_mode != LLCC68_CHIP_MODE_RX)
				printf("ERROR: radio did not enter RX mode\r\n");
		}
	}
}

/** 写入一帧载荷并启动发送；空指针或零长度数据将被忽略。 */
void llcc68_app_send(const uint8_t* data, const uint8_t length)
{
    if ((data == NULL) || (length == 0U)) return;
    llcc68_set_standby(NULL, LLCC68_STANDBY_CFG_RC);
    llcc68_clear_irq_status(NULL, LLCC68_IRQ_ALL);

    /* LLCC68 会按数据包参数中的长度发射，不能始终保留为 255。 */
    lora_packet.pld_len_in_bytes = length;
    llcc68_set_lora_pkt_params(NULL, &lora_packet);
    llcc68_write_buffer(NULL, 0, data, length);
    llcc68_set_tx(NULL, 5000U);
}

/**
 * 读取 DIO1 对应的 IRQ 状态并完成数据搬运。
 * 本函数包含阻塞式 SPI 操作，只能在主循环中调用。
 */
void llcc68_app_process_irq(void)
{
    llcc68_irq_mask_t irq = LLCC68_IRQ_NONE;
    /* 先读取再清除中断，避免同一个事件被重复处理。 */
    if (llcc68_get_irq_status(NULL, &irq) != LLCC68_STATUS_OK)
		return;

	/* 轮询时没有新事件属于正常情况，不能反复重启接收器。 */
	if (irq == LLCC68_IRQ_NONE)
		return;

    llcc68_clear_irq_status(NULL, irq);

    if ((irq & LLCC68_IRQ_TX_DONE) != 0U)
        printf("LoRa TX done\r\n");

    if ((irq & LLCC68_IRQ_TIMEOUT) != 0U)
        printf("LoRa timeout\r\n");

    if ((irq & LLCC68_IRQ_CRC_ERROR) != 0U)
        printf("LoRa RX CRC error\r\n");

    if (((irq & LLCC68_IRQ_RX_DONE) != 0U) && ((irq & LLCC68_IRQ_CRC_ERROR) == 0U))
    {
        /* 查询载荷长度和芯片内部接收缓冲区的起始位置。 */
        llcc68_rx_buffer_status_t rx;
        uint8_t payload[255];
        llcc68_get_rx_buffer_status(NULL, &rx);
        if (rx.pld_len_in_bytes <= sizeof(payload))
        {
            llcc68_read_buffer(NULL, rx.buffer_start_pointer, payload, rx.pld_len_in_bytes);
            /* 先恢复接收；回调函数仍可根据业务需要切换为发送状态。 */
            llcc68_app_start_receive();
            if (rx_callback != NULL)
                rx_callback(payload, rx.pld_len_in_bytes);
            return;
        }
    }
    llcc68_app_start_receive();
}
