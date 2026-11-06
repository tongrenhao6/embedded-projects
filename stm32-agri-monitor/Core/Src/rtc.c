/* rtc.c - RTC free running 1s counter using LSI (40kHz / 40000 = 1Hz) */
#include "rtc.h"

RTC_HandleTypeDef hrtc;

void MX_RTC_Init(void)
{
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = 39999U;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) Error_Handler();

  /* reset counter to zero as epoch start (RTC requires write protection unlock) */
  __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
  RTC->CNTL = 0U;
  RTC->CNTH = 0U;
  __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
}
