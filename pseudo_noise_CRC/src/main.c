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
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_CRCEN | RCC_AHBENR_DMA2EN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM6EN | RCC_APB1ENR_TIM7EN;
}

uint32_t zero_var = 0;

int main(void)
{
	init_clocks();
	set_72MHz();

	//настройка вывода PA4 на выход
	GPIOA->MODER &= ~GPIO_MODER_MODER4;
	GPIOA->MODER |= GPIO_MODER_MODER4_0;

	//генерируем событие TRGO по событию переполнения счётчика
	TIM6->CR2 |= TIM_CR2_MMS_1;
	TIM6->DIER = TIM_DIER_UDE;
	TIM6->ARR = 10;
	TIM6->PSC = 0;

	//генерируем событие TRGO по событию переполнения счётчика
	TIM7->CR2 |= TIM_CR2_MMS_1;
	TIM7->DIER = TIM_DIER_UDE;
	TIM7->ARR = 10;
	TIM7->PSC = 0;

	//настраиваем CRC

	//сброс
	CRC->CR = CRC_CR_RESET;

	//полином 32 бита, без инвертирования данных
	CRC->CR = 0;
	//пишем ненулевое исходное значение
	CRC->INIT = 0xAAAAAAAA;
	//полином, РСЛОС 32 бита, максимальный период
	CRC->POL = 0x000000C5;

	//настраиваем каналы DMA:

	//пересылать по 32 бита в режиме циклического буфера, из памяти в периферию,
	//длина одного цикла равна одной посылке, без прерываний. Пересылка по
	//сигналу от TIM6
	DMA2_Channel3->CCR = DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 | DMA_CCR_CIRC | DMA_CCR_DIR;
	DMA2_Channel3->CNDTR = 1;
	DMA2_Channel3->CMAR = (uint32_t)&zero_var;
	DMA2_Channel3->CPAR = (uint32_t)&(CRC->DR);

	//то же самое, но из "периферии" в "память" (из регистра расчёта CRC CRC->DR
	//в регистр GPIOA->ODR). Пересылка по сигналу от TIM7
	DMA2_Channel4->CCR = DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 | DMA_CCR_CIRC;
	DMA2_Channel4->CNDTR = 1;
	DMA2_Channel4->CMAR = (uint32_t)&(GPIOA->ODR);
	DMA2_Channel4->CPAR = (uint32_t)&(CRC->DR);

	//включаем каналы DMA
	DMA2_Channel3->CCR |= DMA_CCR_EN;
	DMA2_Channel4->CCR |= DMA_CCR_EN;

	//включаем таймеры
	TIM6->CR1 |= TIM_CR1_CEN;
	TIM7->CR1 |= TIM_CR1_CEN;

	while (1) { asm("nop"); }

	return 0;
}
