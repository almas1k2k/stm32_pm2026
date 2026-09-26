#include <stdint.h>
#include <stm32f10x.h>
#include <stdbool.h>

// Пины
#define LED_PIN         13U  // PC13
#define BUTTON_UP_PIN    0U  // PA0 - A
#define BUTTON_DOWN_PIN  1U  // PA1 - C

// Границы частоты 
// delay(1000000) = 1 Гц (полупериод 0.5 сек)
// 64 Гц    → delay = 1000000 / 64  ≈ 15625
// 1/64 Гц  → delay = 1000000 * 64  = 64000000
#define DELAY_MIN   15625U
#define DELAY_MAX   64000000U
#define DELAY_START 1000000U  // 1 Гц

static uint32_t blink_delay = DELAY_START;
static bool A_is_pressed = false;
static bool C_is_pressed = false;

void delay(uint32_t ticks) {
	for (int i=0; i<ticks; i++) {
		__NOP();
	}
}

void led_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1;  // выход, 2 МГц
}

void buttons_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA0 и PA1 — вход с подтяжкой (MODE=00, CNF=10 → 0x8 на каждый пин)
    GPIOA->CRL &= ~0xFFU;
    GPIOA->CRL |=  0x88U;

    // Включаем pull-up: ODR0=1, ODR1=1
    GPIOA->ODR |= (1U << BUTTON_UP_PIN) | (1U << BUTTON_DOWN_PIN);
}

void handle_buttons(void) {
    // PA0 - увеличиваем частоту
    if ((GPIOA->IDR & (1U << BUTTON_UP_PIN)) == 0) {
		if (!A_is_pressed) {
			A_is_pressed = true;
			if (blink_delay > DELAY_MIN) {
				blink_delay /= 2;
				if (blink_delay < DELAY_MIN) blink_delay = DELAY_MIN;
			}
		}
    }
	else if (A_is_pressed) {
		A_is_pressed = false;
	}

    // PA0 - увеньшаем частоту
    if ((GPIOA->IDR & (1U << BUTTON_DOWN_PIN)) == 0) {
		if (!C_is_pressed) {	
			C_is_pressed = true;
			if (blink_delay < DELAY_MAX) {
				blink_delay *= 2;
				if (blink_delay > DELAY_MAX) blink_delay = DELAY_MAX;
			}
		}
    }
	else if (C_is_pressed) {
		C_is_pressed = false;
	}
}


int __attribute((noreturn)) main(void) {
    led_init();
    buttons_init();

    while (1) {
        handle_buttons();

        GPIOC->ODR |=  (1U << LED_PIN);   // LED выключен (active-low)
        delay(blink_delay);
        GPIOC->ODR &= ~(1U << LED_PIN);   // LED горит
        delay(blink_delay);
    }

}
