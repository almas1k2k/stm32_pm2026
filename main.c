#include <stdint.h>
#include <stm32f10x.h>
#include <stdbool.h>

// Пины
// дисплей SPI1
#define DISPLAY_CS_PIN   4U // PA4
#define DISPLAY_DC_PIN   1U // PA1
#define DISPLAY_RES_PIN  0U // PA0

// gamepad
#define BTN_A_PIN    0U  // PB0 (вверх)
#define BTN_C_PIN    1U  // PB1 (вниз)
#define BTN_LEFT_PIN 10U // PB10 (влево / B)
#define BTN_RIGHT_PIN 11U // PB11 (вправо / D)

#define LED_PIN      13U // PC13

#define DELAY_MIN   15625U
#define DELAY_MAX   64000000U
#define DELAY_START 1000000U  // 1 Гц

static uint32_t blink_delay = DELAY_START;
static bool A_is_pressed = false;
static bool C_is_pressed = false;
static bool D_is_pressed = false;
static bool B_is_pressed = false;

volatile uint8_t current_digit = 0;

// Простой шрифт 8x8 для цифр 0-9
const uint8_t font_digits[10][8] = {
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00}, // 0
    {0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00}, // 1
    {0x3C, 0x42, 0x04, 0x08, 0x10, 0x20, 0x7E, 0x00}, // 2
    {0x3C, 0x42, 0x04, 0x18, 0x04, 0x42, 0x3C, 0x00}, // 3
    {0x08, 0x18, 0x28, 0x48, 0x7E, 0x08, 0x08, 0x00}, // 4
    {0x7E, 0x40, 0x7C, 0x02, 0x02, 0x42, 0x3C, 0x00}, // 5
    {0x3C, 0x40, 0x7C, 0x42, 0x42, 0x42, 0x3C, 0x00}, // 6
    {0x7E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00}, // 7
    {0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00}, // 8
    {0x3C, 0x42, 0x42, 0x42, 0x3E, 0x02, 0x3C, 0x00}  // 9
};

// задержка для антидребезга
void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 1000; i++) {
        __NOP();
    }
}

// задержка для мигания
void delay(uint32_t ticks) {
	for (int i=0; i<ticks; i++) {
		__NOP();
	}
}

// =========== SPI

void SPI1_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN | RCC_APB2ENR_AFIOEN;
	
    GPIOA->CRL &= ~(0xFFFF0000U); 
    GPIOA->CRL |=  (0xBB200000U); 

    GPIOA->CRL &= ~(0x000000FFU); 
    GPIOA->CRL |=  (0x00000022U); 

    SPI1->CR1 = 0; 
    SPI1->CR1 |= SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_MSTR | 
                 SPI_CR1_BR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE;
}

void SPI1_Write(uint8_t data) {
    while (!(SPI1->SR & SPI_SR_TXE)); // ждем готовности буфера
    SPI1->DR = data;                  // отправляем
}

uint8_t SPI1_Read(void) {
    SPI1->DR = 0; 
    while (!(SPI1->SR & SPI_SR_RXNE));
    return SPI1->DR;
}

// ============= управление дисплеем

void display_cmd(uint8_t cmd) {
    GPIOA->ODR &= ~(1U << DISPLAY_CS_PIN); // CS = 0
    GPIOA->ODR &= ~(1U << DISPLAY_DC_PIN); // DC = 0 (команда)
    delay_ms(1);
    SPI1_Write(cmd);
    GPIOA->ODR |= (1U << DISPLAY_CS_PIN);  // CS = 1
}

void display_data(uint8_t data) {
    GPIOA->ODR &= ~(1U << DISPLAY_CS_PIN); // CS = 0
    GPIOA->ODR |= (1U << DISPLAY_DC_PIN);  // DC = 1 (данные)
    delay_ms(1);
    SPI1_Write(data);
    GPIOA->ODR |= (1U << DISPLAY_CS_PIN);  // CS = 1
}

void display_init(void) {
    GPIOA->ODR &= ~(1U << DISPLAY_RES_PIN); 
    delay_ms(10);
    GPIOA->ODR |= (1U << DISPLAY_RES_PIN);  
    delay_ms(10);

	// стандартная инициализация для ssd1306
    display_cmd(0xAE); 
    display_cmd(0x20); display_cmd(0x00); 
    display_cmd(0xB0); 
    display_cmd(0xC8); 
    display_cmd(0x00); display_cmd(0x10); 
    display_cmd(0x40); 
    display_cmd(0x81); display_cmd(0x7F); 
    display_cmd(0xA1); 
    display_cmd(0xA6); 
    display_cmd(0xA8); display_cmd(0x3F); 
    display_cmd(0xA4); 
    display_cmd(0xD3); display_cmd(0x00); 
    display_cmd(0xD5); display_cmd(0x80);
    display_cmd(0xD9); display_cmd(0xF1); 
    display_cmd(0xDA); display_cmd(0x12); 
    display_cmd(0xDB); display_cmd(0x40); 
    display_cmd(0x8D); display_cmd(0x14); 
    display_cmd(0xAF); 
}

// ============ отрисовка цифры

void draw_digit(uint8_t digit) {
    display_cmd(0xB0 + 3); // выбор страницы
    display_cmd(0x00 | (60 & 0x0F)); 
    display_cmd(0x10 | (60 >> 4));   
    
    for (int i = 0; i < 8; i++) {
        display_data(font_digits[digit][i]);
    }
}

// =========== Таймер

void Timer1_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    
    // 72MHz / 7200 = 10 000 Гц. 10 000 / 10 000 = 1 Гц (1 секунда)
    TIM1->PSC = 7199; 
    TIM1->ARR = 9999; 
    
    TIM1->DIER |= TIM_DIER_UIE; // разрешаем прерывание по переполнению
    TIM1->BDTR |= TIM_BDTR_MOE; 
    
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    
    TIM1->CR1 |= TIM_CR1_CEN;
}

// Обработчик прерывания таймера
void TIM1_UP_IRQHandler(void) {
    if (TIM1->SR & TIM_SR_UIF) {
        TIM1->SR &= ~TIM_SR_UIF; // сбрасываем флаг прерывания
        
        current_digit++;
        if (current_digit > 9) current_digit = 0;
        
        draw_digit(current_digit); // обновляем цифру на экране
    }
}

// ============ отрисовка шахматной доски

void draw_chessboard(void) {
    for (uint8_t page = 0; page < 8; page++) {
        display_cmd(0xB0 + page); 
        display_cmd(0x00);        
        display_cmd(0x10);        

        for (uint8_t block = 0; block < 8; block++) { 
            // Если сумма страницы и блока четная - белый, иначе черный
            uint8_t color = ((page + block) % 2 == 0) ? 0xFF : 0x00; 
            for (uint8_t i = 0; i < 16; i++) {
                display_data(color); 
            }
        }
    }
}

// =========== Кнопки и светодиод	

void led_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1; 
}

void buttons_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    // PB0, PB1 (CRL)
    GPIOB->CRL &= ~0x000000FFU;
    GPIOB->CRL |=  0x00000088U; 
    // PB10, PB11 (CRH)
    GPIOB->CRH &= ~0x0000FF00U;
    GPIOB->CRH |=  0x00008800U; 

    GPIOB->ODR |= (1U << BTN_A_PIN) | (1U << BTN_C_PIN) | (1U << BTN_LEFT_PIN) | (1U << BTN_RIGHT_PIN);
}

void handle_buttons(void) {
    // A (PB0)
    if ((GPIOB->IDR & (1U << BTN_A_PIN)) == 0) {
        if (!A_is_pressed) {
            A_is_pressed = true;
            delay_ms(20); 
            if (blink_delay > DELAY_MIN) {
                blink_delay /= 2;
                if (blink_delay < DELAY_MIN) blink_delay = DELAY_MIN;
            }
        }
    } else if (A_is_pressed) A_is_pressed = false;

    // C (PB1)
    if ((GPIOB->IDR & (1U << BTN_C_PIN)) == 0) {
        if (!C_is_pressed) {
            C_is_pressed = true;
            delay_ms(20); 
            if (blink_delay < DELAY_MAX) {
                blink_delay *= 2;
                if (blink_delay > DELAY_MAX) blink_delay = DELAY_MAX;
            }
        }
    } else if (C_is_pressed) C_is_pressed = false;

    // B (PB10)
    if ((GPIOB->IDR & (1U << BTN_LEFT_PIN)) == 0) {
        if (!D_is_pressed) {
            D_is_pressed = true;
            delay_ms(20);
            blink_delay = DELAY_MAX;
        }
    } else if (D_is_pressed) D_is_pressed = false;

    // D (PB11)
    if ((GPIOB->IDR & (1U << BTN_RIGHT_PIN)) == 0) {
        if (!B_is_pressed) {
            B_is_pressed = true;
            delay_ms(20);
            blink_delay = DELAY_MIN;
        }
    } else if (B_is_pressed) B_is_pressed = false;
}

// ========== main ========

int __attribute((noreturn)) main(void) {
    led_init();
    buttons_init();
    SPI1_Init();
    display_init();
    
    draw_digit(0); // Выводим начальную цифру 0
    Timer1_Init(); // Запускаем таймер
    
    while (1) {
        handle_buttons();
        
        GPIOC->ODR |=  (1U << LED_PIN);  
        delay(blink_delay);
        GPIOC->ODR &= ~(1U << LED_PIN);   
        delay(blink_delay);
    }
}
