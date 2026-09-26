#include <stdint.h>
#include <stm32f10x.h>

void delay(uint32_t ticks) {
	for (int i=0; i<ticks; i++) {
		__NOP();
	}
}

int __attribute((noreturn)) main(void) {
	// Enable clock for AFIO
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
	// Enable clock for GPIOC
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
	// Enable PC13 push-pull mode
	GPIOC->CRH &= ~GPIO_CRH_CNF13; //clear cnf bits
	GPIOC->CRH |= GPIO_CRH_MODE13_0; //Max speed = 10Mhz

    uint32_t period = 2000000;

    while (1) {
        // --- Фаза "ускорение" ---
        // Уменьшаем period на 200000 каждую итерацию, пока не дойдём до 100000
        for (period = 2000000; period > 100000; period -= 200000) {
            GPIOC->ODR |=  (1U << 13U);  // LED выключен
            delay(period);
            GPIOC->ODR &= ~(1U << 13U);  // LED горит
            delay(period);
        }

        // --- Фаза "замедление" ---
        // Увеличиваем period обратно
        for (period = 100000; period < 2000000; period += 200000) {
            GPIOC->ODR |=  (1U << 13U);
            delay(period);
            GPIOC->ODR &= ~(1U << 13U);
            delay(period);
        }
    }
}
