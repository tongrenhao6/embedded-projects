/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "oled.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MODE_MANUAL 0
#define MODE_AUTO   1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/*记录 LED1、LED2 当前开关状态*/
uint8_t led1_state = 0;
uint8_t led2_state = 0;
// 记录当前被选中的 LED，是 1 还是 2
uint8_t selected_led = 1;
// 记录亮度模式，0 是手动，1 是自动
uint8_t brightness_mode = MODE_MANUAL;
// 记录主灯当前亮度百分比，范围 0~100，初始为50
uint16_t brightness = 50;
// 记录光敏传感器的 ADC 原始值
uint16_t light_raw = 0;
// 记录上一次编码器计数值，用来判断旋转方向和步进
uint16_t encoder_last_count = 0;
// 用于限制自动调光和串口打印频率，避免刷新过快
uint32_t auto_adjust_tick = 0;
uint32_t auto_report_tick = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Panel_ShowStatus(void);
void MainLight_SetBrightness(uint16_t percent);
void Serial_Print(char *text);
void Serial_PrintStatus(char *event_name);
uint16_t LightSensor_Read(void);
void BrightnessMode_Toggle(void);
void AutoBrightness_Update(uint8_t force_apply, uint8_t report_event);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 串口发送基础字符串，用于输出调试信息 */
void Serial_Print(char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, strlen(text), HAL_MAX_DELAY);
}

/* 按统一格式输出当前系统状态，便于串口调试观察 */
void Serial_PrintStatus(char *event_name)
{
  char buffer[128];

  /* 将当前模式、选中对象、灯状态、主灯亮度和光照值拼成一行日志 */
  snprintf(buffer, sizeof(buffer),
           "[%-8s] Mode=%s | Sel=L%d | LED1=%s | LED2=%s | Main=%3u%% | Light=%4u\r\n",
           event_name,
           brightness_mode == MODE_AUTO ? "AUTO" : "MANUAL",
           selected_led,
           led1_state ? "ON" : "OFF",
           led2_state ? "ON" : "OFF",
           brightness,
           light_raw);
  Serial_Print(buffer);
}

/* 按百分比设置主灯亮度，本质上是修改 PWM 占空比 */
void MainLight_SetBrightness(uint16_t percent)
{
  uint32_t pulse;

  /* 对外只允许设置 0~100 的亮度百分比 */
  if (percent > 100U)
  {
    percent = 100U;
  }

  brightness = percent;
  /* 将亮度百分比换算为 TIM2 的比较值，从而改变 PWM 占空比 */
  pulse = ((uint32_t)htim2.Init.Period * brightness) / 100U;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
}

/* 启动一次 ADC 转换并读取光敏传感器的原始值 */
uint16_t LightSensor_Read(void)
{
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
  return (uint16_t)HAL_ADC_GetValue(&hadc1);
}

/* 在手动模式和自动模式之间切换 */
void BrightnessMode_Toggle(void)
{
  if (brightness_mode == MODE_MANUAL)
  {
    brightness_mode = MODE_AUTO;
  }
  else
  {
    brightness_mode = MODE_MANUAL;
  }
}

/* 自动模式下根据环境光强更新主灯亮度，并限制更新频率避免抖动 */
void AutoBrightness_Update(uint8_t force_apply, uint8_t report_event)
{
  uint16_t auto_brightness;
  uint32_t now_tick;

  /* 将 12 位 ADC 值映射到 20%~100% 的亮度范围，避免自动模式下过暗 */
  auto_brightness = 20U + (light_raw * 80U) / 4095U;
  now_tick = HAL_GetTick();

  /* 满足强制更新，或满足“时间间隔 + 亮度变化阈值”时才真正刷新 */
  if (force_apply ||
      (((now_tick - auto_adjust_tick) >= 200U) &&
       ((auto_brightness > brightness ? (auto_brightness - brightness) : (brightness - auto_brightness)) >= 5U)))
  {
    MainLight_SetBrightness(auto_brightness);
    Panel_ShowStatus();
    auto_adjust_tick = now_tick;

    if (report_event && ((now_tick - auto_report_tick) >= 400U))
    {
      /* 自动模式下不必每次都打串口，限频输出更便于观察 */
      Serial_PrintStatus("AUTO");
      auto_report_tick = now_tick;
    }
  }
}

/* 将当前选中的 LED、开关状态、亮度和模式刷新到 OLED 上 */
void Panel_ShowStatus(void)
{
  /* 第 1 行显示 LED1 状态，> 表示当前被选中 */
  if (selected_led == 1)
  {
    OLED_ShowString(1, 1, ">L1:");
  }
  else
  {
    OLED_ShowString(1, 1, " L1:");
  }

  if (led1_state)
  {
    OLED_ShowString(1, 6, "ON ");
  }
  else
  {
    OLED_ShowString(1, 6, "OFF");
  }

  /* 第 2 行显示 LED2 状态，> 表示当前被选中 */
  if (selected_led == 2)
  {
    OLED_ShowString(2, 1, ">L2:");
  }
  else
  {
    OLED_ShowString(2, 1, " L2:");
  }

  if (led2_state)
  {
    OLED_ShowString(2, 6, "ON ");
  }
  else
  {
    OLED_ShowString(2, 6, "OFF");
  }

  /* 第 3 行显示主灯亮度百分比 */
  OLED_ShowString(3, 1, "Bright:");
  OLED_ShowNum(3, 8, brightness, 3);
  OLED_ShowChar(3, 11, '%');

  /* 第 4 行显示当前工作模式 */
  if (brightness_mode == MODE_AUTO)
  {
    OLED_ShowString(4, 1, "Mode: AUTO  ");
  }
  else
  {
    OLED_ShowString(4, 1, "Mode: MANUAL");
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 对 ADC 进行校准，保证光敏采样更稳定 */
  HAL_ADCEx_Calibration_Start(&hadc1);
  
  /* 启动主灯 PWM 输出 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  
  /* 启动编码器接口，用于读取旋转方向和步数 */
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  
  /* 编码器计数清零，作为初始参考值 */
  __HAL_TIM_SET_COUNTER(&htim3, 0);
  encoder_last_count = 0;
  auto_adjust_tick = HAL_GetTick();
  auto_report_tick = HAL_GetTick();
  
  /* 上电默认关闭两路普通 LED */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
  
  /* 主灯默认亮度设为 55% */
  MainLight_SetBrightness(55);
  
  /* 初始化 OLED 并显示默认状态 */
  OLED_Init();
  Panel_ShowStatus();
  
  /* 串口打印启动信息，方便联调 */
  Serial_Print("\r\n================ Smart Home Panel ================\r\n");
  Serial_PrintStatus("START");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 周期性读取光敏传感器的 ADC 原始值 */
    light_raw = LightSensor_Read();

    /* 若当前处于自动模式，则根据环境光自动调整主灯亮度 */
    if (brightness_mode == MODE_AUTO)
    {
      AutoBrightness_Update(0, 1);
    }

    {
      /* 读取编码器计数变化，用于手动调节亮度 */
      uint16_t encoder_count = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
      /* 当前计数减去上一次计数，即可得到旋转方向和步进变化 */
      int16_t encoder_delta = (int16_t)(encoder_count - encoder_last_count);
      if (encoder_delta != 0)
      {
        /* 自动模式下如果用户手动旋转编码器，则切回手动模式 */
        if (brightness_mode == MODE_AUTO)
        {
          brightness_mode = MODE_MANUAL;
          Panel_ShowStatus();
          Serial_Print("Manual override\r\n");
        }

        /* 根据旋转方向按 5% 步进增减主灯亮度 */
        if (encoder_delta > 0)
        {
          /* 顺时针方向：提高亮度，上限限制为 100% */
          if (brightness <= 95U)
          {
            MainLight_SetBrightness(brightness + 5U);
          }
          else
          {
            MainLight_SetBrightness(100U);
          }
        }
        else
        {
          /* 逆时针方向：降低亮度，下限限制为 0% */
          if (brightness >= 5U)
          {
            MainLight_SetBrightness(brightness - 5U);
          }
          else
          {
            MainLight_SetBrightness(0U);
          }
        }
        /* 保存本次计数值，作为下一次比较基准 */
        encoder_last_count = encoder_count;
        Panel_ShowStatus();
        Serial_PrintStatus("BRIGHT");
      }
    }

    /* 按键 PA4：切换当前选中的 LED1 / LED2 */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
    {
      /* 简单软件消抖：延时后再次确认按键仍然按下 */
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
      {
        if (selected_led == 1)
        {
          selected_led = 2;
        }
        else
        {
          selected_led = 1;
        }
        Panel_ShowStatus();
        Serial_PrintStatus("SELECT");
        /* 等待按键释放，避免一次长按被识别为多次触发 */
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
        {
        }
        HAL_Delay(20);
      }
    }

    /* 按键 PB13：控制当前选中 LED 的开关状态 */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET)
    {
      /* 先消抖，再执行状态翻转 */
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET)
      {
        if (selected_led == 1)
        {
          /* 如果当前选中 LED1，就翻转 LED1 的软件状态并更新引脚输出 */
          led1_state = !led1_state;
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, led1_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
        else
        {
          /* 如果当前选中 LED2，就翻转 LED2 的软件状态并更新引脚输出 */
          led2_state = !led2_state;
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, led2_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
        Panel_ShowStatus();
        Serial_PrintStatus("SWITCH");
        while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET)
        {
        }
        HAL_Delay(20);
      }
    }

    /* 按键 PB14：切换主灯亮度模式（手动 / 自动） */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
    {
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
      {
        /* 先切换模式变量，再决定后续是刷新显示还是自动应用亮度 */
        BrightnessMode_Toggle();
        /* 切换到自动模式后立即重新采样并应用一次亮度 */
        light_raw = LightSensor_Read();
        if (brightness_mode == MODE_AUTO)
        {
          AutoBrightness_Update(1, 0);
        }
        else
        {
          /* 切回手动模式时不主动改亮度，只更新显示内容 */
          Panel_ShowStatus();
        }
        Serial_PrintStatus("MODE");
        while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
        {
        }
        HAL_Delay(20);
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
