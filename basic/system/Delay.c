#include "stm32f10x.h"

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~4294967295；较长延时会分段等待
  * @retval 无
  */
void Delay_us(uint32_t xus)
{
	uint32_t cycles_per_us = SystemCoreClock / 1000000U;
	uint32_t max_chunk;

	if (xus == 0U || cycles_per_us == 0U) return;
	max_chunk = 0x00FFFFFFU / cycles_per_us;
	while (xus != 0U)
	{
		uint32_t chunk = (xus > max_chunk) ? max_chunk : xus;
		SysTick->CTRL = 0U;
		SysTick->LOAD = (cycles_per_us * chunk) - 1U;
		SysTick->VAL = 0U;
		SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
		while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U) { }
		SysTick->CTRL = 0U;
		xus -= chunk;
	}
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);
	}
}
 
/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	while(xs--)
	{
		Delay_ms(1000);
	}
} 
