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
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_DMA1EN | RCC_AHBENR_ADC12EN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
}

int main(void)
{
	init_clocks();
	set_72MHz();

	//настройка вывода PA0 на выход
	GPIOA->MODER &= ~GPIO_MODER_MODER0;
	GPIOA->MODER |= GPIO_MODER_MODER0_0;

	//настройка TIM2: TRGO по переполнению счётчика
	TIM2->CR2 = TIM_CR2_MMS_1;
	TIM2->ARR = 72;
	TIM2->PSC = 0;

	//настройка АЦП: DMA в циклическом режиме, разрешение 12 бит, преобразование
	//по сигналу от TIM2_TRGO, включаем и выбираем внутренний температурный датчик (16-ый канал).
	//Discontinuous mode для работы с DMA. Работаем только с одним каналом (температурный датчик).
	//Запуск преобразования по сигналу от TIM2
	ADC1->CFGR = ADC_CFGR_DMACFG | ADC_CFGR_DMAEN | ADC_CFGR_DISCEN | ADC_CFGR_EXTEN_0 |
			ADC_CFGR_EXTSEL_3 | ADC_CFGR_EXTSEL_1 | ADC_CFGR_EXTSEL_0 | ADC_CFGR_DISCEN;
	ADC1->SQR1 = ADC_SQR1_SQ1_4;
	ADC1_2_COMMON->CCR = ADC12_CCR_TSEN | ADC12_CCR_CKMODE_1;

	//настройка DMA: по 16 бит, циклический режим, без инкремента адресов, из ADC1->DR в GPIOA->ODR
	DMA1_Channel1->CCR = DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | DMA_CCR_CIRC;
	DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
	DMA1_Channel1->CMAR = (uint32_t)&GPIOA->ODR;
	DMA1_Channel1->CNDTR = 1;

	//включаем DMA
	DMA1_Channel1->CCR |= DMA_CCR_EN;
	//включаем АЦП
	ADC1->CR |= ADC_CR_ADEN;
	ADC1->CR |= ADC_CR_ADSTART;
	//включаем таймер
	TIM2->CR1 |= TIM_CR1_CEN;

	while (1) { asm("nop"); }

	return 0;
}
