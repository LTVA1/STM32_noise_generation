#include "stm32h743xx.h"

void init_clocks()
{
	//включаем HSI48, внутренний генератор 48 МГц, от которого
	//по умолчанию тактируется генератор
	//случайных чисел
	RCC->CR |= RCC_CR_HSI48ON;

	//ждём готовности HSI48
	while(!(RCC->CR & RCC_CR_HSI48RDY)) { asm("nop"); }

	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
	RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;

	RCC->APB1LENR |= RCC_APB1LENR_TIM6EN;

	//сброс генератора случайных чисел
	RCC->AHB2RSTR |= RCC_AHB2RSTR_RNGRST;
	RCC->AHB2RSTR &= ~RCC_AHB2RSTR_RNGRST;

	//ждём несколько тактов, чтобы для периферии
	//правильно включилось тактирование
	asm("nop");
	asm("nop");
	asm("nop");
}

int main(void)
{
	init_clocks();

	//включение генератора случайных чисел
	//заранее, поскольку время генерации первого
	//набора случайных чисел выше, чем время
	//генерации следующих наборов
	RNG->CR |= RNG_CR_RNGEN;

	//настройка вывода PA0 на выход
	GPIOA->MODER &= ~GPIO_MODER_MODER0;
	GPIOA->MODER |= GPIO_MODER_MODER0_0;

	//настройка таймера
	//генерируем событие TRGO по событию переполнения счётчика
	TIM6->CR2 |= TIM_CR2_MMS_1;
	TIM6->DIER = TIM_DIER_UDE;
	TIM6->ARR = 64 - 1;
	TIM6->PSC = 10 - 1;

	//настраиваем DMA:
	//нулевой канал, пересылать по 32 бита в режиме циклического буфера,
	//из "периферии" в "память", циклический режим по одной посылке, без
	//прерываний, пересылаем из RNG->DR в GPIOA->ODR
	DMA1_Stream0->CR = DMA_SxCR_CIRC | DMA_SxCR_MSIZE_1 |
			DMA_SxCR_PSIZE_1;
	DMA1_Stream0->PAR = (uint32_t)&RNG->DR;
	DMA1_Stream0->M0AR = (uint32_t)&GPIOA->ODR;
	DMA1_Stream0->NDTR = 1;

	//настраиваем DMAMUX:
	//нулевой канал по запросу от TIM6
	//69 - TIM6_UP
	DMAMUX1_Channel0->CCR = 69;

	//включаем DMA
	DMA1_Stream0->CR |= DMA_SxCR_EN;
	//включаем таймер
	TIM6->CR1 |= TIM_CR1_CEN;

	while (1) { asm("nop"); }

	return 0;
}
