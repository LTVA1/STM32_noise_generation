#include "stm32f3xx.h"

//маска битов, которые являются отводами РСЛОС
//https://docs.amd.com/v/u/en-US/xapp052 для 32 бит
//биты в обратном порядке, поскольку РСЛОС сдвигается
//в обратном направлении
#define LFSR_MASK (1 | (1 << 10) | (1 << 30) | (1 << 31))

//переменная, которая хранит состояние РСЛОС
uint32_t lfsr;

//переменная, которая хранит состояние
//младшего бита РСЛОС до сдвига
uint32_t feedback;

//обработчик прерывания TIM2
void TIM2_IRQHandler()
{
	//если выставлен флаг прерывания по переполнению счётчика
	if(TIM2->SR & TIM_SR_UIF)
	{
		//то сбрасываем флаг
		TIM2->SR &= ~TIM_SR_UIF;

		//сохраняем младший бит
		feedback = lfsr & 1;

		//сдвиг содержимого РСЛОС
		lfsr >>= 1;

		//если младший бит равен единице
		if(feedback)
		{
			//XOR с маской битов отводов
			//все биты, кроме битов отводов,
			//не меняются.
			//Биты отводов "переворачиваются"
			lfsr ^= LFSR_MASK;
		}

		//записываем в регистр BSRR бит в зависимости от состояния младшего бита РСЛОС
		//*(volatile uint32_t*)... по причине того, что в версии CMSIS под этот
		//микроконтроллер регистр BSRR объявлен как два 16-битных поля, в то время
		//как обращаться к нему удобнее как к 32-битному регистру
		*(volatile uint32_t*)&(GPIOA->BSRRL) = feedback ? 1 : (1 << 16);
	}
}

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
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
}

int main(void)
{
	init_clocks();
	set_72MHz();

	//настройка вывода PA0 на выход
	GPIOA->MODER &= ~GPIO_MODER_MODER0;
	GPIOA->MODER |= GPIO_MODER_MODER0_0;

	//настройка TIM2: прерывание по переполнению счётчика
	TIM2->DIER = TIM_DIER_UIE;
	TIM2->ARR = 72 - 1;
	TIM2->PSC = 10 - 1;

	//записываем ненулевое число в переменную, хранящую состояние РСЛОС
	lfsr = 222;

	//включаем прерывание
	NVIC_EnableIRQ(TIM2_IRQn);
	//запуск таймера
	TIM2->CR1 |= TIM_CR1_CEN;

	while (1) { asm("nop"); }

	return 0;
}
