#include "oled.h"
#include "oled_font.h"

#define OLED_SCL_PORT GPIOB
#define OLED_SCL_PIN  GPIO_PIN_8
#define OLED_SDA_PORT GPIOB
#define OLED_SDA_PIN  GPIO_PIN_9

static void OLED_WriteSCL(GPIO_PinState state)
{
	HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, state);
}

static void OLED_WriteSDA(GPIO_PinState state)
{
	HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, state);
}

static void OLED_GPIO_Init(void)
{
	GPIO_InitTypeDef gpio_init = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();

	gpio_init.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
	gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
	gpio_init.Pull = GPIO_NOPULL;
	gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &gpio_init);

	OLED_WriteSCL(GPIO_PIN_SET);
	OLED_WriteSDA(GPIO_PIN_SET);
}

static void OLED_I2C_Start(void)
{
	OLED_WriteSDA(GPIO_PIN_SET);
	OLED_WriteSCL(GPIO_PIN_SET);
	OLED_WriteSDA(GPIO_PIN_RESET);
	OLED_WriteSCL(GPIO_PIN_RESET);
}

static void OLED_I2C_Stop(void)
{
	OLED_WriteSDA(GPIO_PIN_RESET);
	OLED_WriteSCL(GPIO_PIN_SET);
	OLED_WriteSDA(GPIO_PIN_SET);
}

static void OLED_I2C_SendByte(uint8_t byte)
{
	uint8_t i;

	for (i = 0; i < 8; i++)
	{
		OLED_WriteSDA((byte & (0x80U >> i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		OLED_WriteSCL(GPIO_PIN_SET);
		OLED_WriteSCL(GPIO_PIN_RESET);
	}

	OLED_WriteSCL(GPIO_PIN_SET);
	OLED_WriteSCL(GPIO_PIN_RESET);
}

static void OLED_WriteCommand(uint8_t command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x00);
	OLED_I2C_SendByte(command);
	OLED_I2C_Stop();
}

static void OLED_WriteData(uint8_t data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x40);
	OLED_I2C_SendByte(data);
	OLED_I2C_Stop();
}

static void OLED_SetCursor(uint8_t y, uint8_t x)
{
	OLED_WriteCommand(0xB0 | y);
	OLED_WriteCommand(0x10 | ((x & 0xF0U) >> 4));
	OLED_WriteCommand(0x00 | (x & 0x0FU));
}

static uint32_t OLED_Pow(uint32_t x, uint32_t y)
{
	uint32_t result = 1;

	while (y--)
	{
		result *= x;
	}

	return result;
}

void OLED_Clear(void)
{
	uint8_t i;
	uint8_t j;

	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for (i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}

void OLED_ShowChar(uint8_t line, uint8_t column, char ch)
{
	uint8_t i;

	OLED_SetCursor((line - 1U) * 2U, (column - 1U) * 8U);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[(uint8_t)ch - ' '][i]);
	}

	OLED_SetCursor((line - 1U) * 2U + 1U, (column - 1U) * 8U);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[(uint8_t)ch - ' '][i + 8U]);
	}
}

void OLED_ShowString(uint8_t line, uint8_t column, char *string)
{
	uint8_t i;

	for (i = 0; string[i] != '\0'; i++)
	{
		OLED_ShowChar(line, column + i, string[i]);
	}
}

void OLED_ShowNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
	uint8_t i;

	for (i = 0; i < length; i++)
	{
		OLED_ShowChar(line, column + i,
		              number / OLED_Pow(10U, length - i - 1U) % 10U + '0');
	}
}

void OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t number, uint8_t length)
{
	uint8_t i;
	uint32_t abs_number;

	if (number >= 0)
	{
		OLED_ShowChar(line, column, '+');
		abs_number = (uint32_t)number;
	}
	else
	{
		OLED_ShowChar(line, column, '-');
		abs_number = (uint32_t)(-number);
	}

	for (i = 0; i < length; i++)
	{
		OLED_ShowChar(line, column + i + 1U,
		              abs_number / OLED_Pow(10U, length - i - 1U) % 10U + '0');
	}
}

void OLED_ShowHexNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
	uint8_t i;
	uint8_t single_number;

	for (i = 0; i < length; i++)
	{
		single_number = number / OLED_Pow(16U, length - i - 1U) % 16U;
		if (single_number < 10U)
		{
			OLED_ShowChar(line, column + i, single_number + '0');
		}
		else
		{
			OLED_ShowChar(line, column + i, single_number - 10U + 'A');
		}
	}
}

void OLED_ShowBinNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
	uint8_t i;

	for (i = 0; i < length; i++)
	{
		OLED_ShowChar(line, column + i,
		              number / OLED_Pow(2U, length - i - 1U) % 2U + '0');
	}
}

void OLED_Init(void)
{
	OLED_GPIO_Init();
	HAL_Delay(100);

	OLED_WriteCommand(0xAE);
	OLED_WriteCommand(0xD5);
	OLED_WriteCommand(0x80);
	OLED_WriteCommand(0xA8);
	OLED_WriteCommand(0x3F);
	OLED_WriteCommand(0xD3);
	OLED_WriteCommand(0x00);
	OLED_WriteCommand(0x40);
	OLED_WriteCommand(0xA1);
	OLED_WriteCommand(0xC8);
	OLED_WriteCommand(0xDA);
	OLED_WriteCommand(0x12);
	OLED_WriteCommand(0x81);
	OLED_WriteCommand(0xCF);
	OLED_WriteCommand(0xD9);
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);
	OLED_WriteCommand(0xA6);
	OLED_WriteCommand(0x8D);
	OLED_WriteCommand(0x14);
	OLED_WriteCommand(0xAF);

	OLED_Clear();
}
