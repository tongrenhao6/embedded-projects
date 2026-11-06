/* dma.c - DMA1 clock enable; ADC DMA handle is set up in HAL_ADC_MspInit */
#include "dma.h"

void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();
}
