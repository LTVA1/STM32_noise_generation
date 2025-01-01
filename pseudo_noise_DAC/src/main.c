#include "stm32f3xx.h"

void set_72MHz()
{
	RCC->CR |= ((uint32_t)RCC_CR_HSEON);

	/* Ждем пока HSE не выставит бит готовности */
	while(!(RCC->CR & RCC_CR_HSERDY)) {}

	/* Конфигурируем Flash на 2 цикла ожидания */
	/* Flash не может работать на высокой частоте */
	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_2;

	/* HCLK = SYSCLK */
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;

	/* PCLK2 = HCLK */
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;

	/* PCLK1 = HCLK / 2 */
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

	/* Конфигурируем множитель PLL configuration: PLLCLK = HSE * 9 = 72 MHz */
	/* При условии, что кварц на 8МГц! */
	RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMUL);
	RCC->CFGR |= RCC_CFGR_PLLSRC_HSE_PREDIV | RCC_CFGR_PLLMUL9;

	/* Включаем PLL */
	RCC->CR |= RCC_CR_PLLON;

	/* Ожидаем, пока PLL выставит бит готовности */
	while((RCC->CR & RCC_CR_PLLRDY) == 0) { asm("nop"); }

	/* Выбираем PLL как источник системной частоты */
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	/* Ожидаем, пока PLL выберется как источник системной частоты */
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { asm("nop"); }

	SystemCoreClockUpdate();
}

void init_clocks()
{
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	RCC->APB1ENR |= RCC_APB1ENR_DAC1EN | RCC_APB1ENR_TIM6EN;
}

int main(void)
{
	init_clocks();
	set_72MHz();

	//аналоговая функция вывода PA4
	GPIOA->MODER |= GPIO_MODER_MODER4;

	//генерируем событие TRGO по событию переполнения счётчика
	TIM6->CR2 |= TIM_CR2_MMS_1;
	TIM6->ARR = 72 - 1;
	TIM6->PSC = 2 - 1;

	//Настраиваем ЦАП: максимальная амплитуда шума, выбор шума в
	//качестве типа генерируемой волны, включаем trigger
	//DAC_CR_TSEL1 = 0 для TIM6 в качестве источника тактирования
	//генератора шума
	DAC->CR = DAC_CR_MAMP1 | DAC_CR_WAVE1_0 | DAC_CR_TEN1;

	//включаем первый канал ЦАПа
	DAC->CR |= DAC_CR_EN1;
	//включаем таймер
	TIM6->CR1 |= TIM_CR1_CEN;

	while (1) { asm("nop"); }

	return 0;
}
