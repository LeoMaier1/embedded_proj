#include <stm32f0xx.h>
#include "clock_.h"
#include "fifo.h"

// Select the Baudrate for the UART
#define BAUDRATE 115200 // Baud rate set to 9600 baud per second

volatile Fifo_t usart_rx_fifo;

const uint8_t USART2_RX_PIN = 3; // PA3 is used as USART2_RX
const uint8_t USART2_TX_PIN = 2; // PA2 is used as USART2_TX

void uart_init(void) {
    SystemClock_Config(); // Configure the system clock to 48 MHz

    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;    // Enable GPIOA clock
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN; // Enable USART2 clock

    GPIOA->MODER |= 0b10 << (USART2_TX_PIN * 2);    // Set PA2 to Alternate Function mode
    GPIOA->AFR[0] |= 0b0001 << (4 * USART2_TX_PIN); // Set AF for PA2 (USART2_TX)
    GPIOA->MODER |= 0b10 << (USART2_RX_PIN * 2);    // Set PA3 to Alternate Function mode
    GPIOA->AFR[0] |= 0b0001 << (4 * USART2_RX_PIN); // Set AF for PA3 (USART2_RX)

    USART2->BRR = (APB_FREQ / BAUDRATE); // Set baud rate (requires APB_FREQ to be defined)
    USART2->CR1 |= 0b1 << 2;             // Enable receiver (RE bit)
    USART2->CR1 |= 0b1 << 3;             // Enable transmitter (TE bit)
    USART2->CR1 |= 0b1 << 0;             // Enable USART (UE bit)
    USART2->CR1 |= 0b1 << 5;             // Enable RXNE interrupt (RXNEIE bit)

    NVIC_SetPriorityGrouping(0);                               // Use 4 bits for priority, 0 bits for subpriority
    uint32_t uart_pri_encoding = NVIC_EncodePriority(0, 1, 0); // Encode priority: group 1, subpriority 0
    NVIC_SetPriority(USART2_IRQn, uart_pri_encoding);          // Set USART2 interrupt priority
    NVIC_EnableIRQ(USART2_IRQn);                               // Enable USART2 interrupt

    fifo_init((Fifo_t *)&usart_rx_fifo);                       // Init the FIFO
}

void USART2_IRQHandler(void) {
    if (USART2->ISR & USART_ISR_RXNE) { // Check if RXNE flag is set (data received)
        uint8_t c = USART2->RDR;       // Read received byte from RDR
        fifo_put((Fifo_t *)&usart_rx_fifo, c); // Put incoming data into the FIFO buffer
    }
}

void uart_write_char(int c) {
    while (!(USART2->ISR & USART_ISR_TXE));
    USART2->TDR = c; 
}

void uart_write_string(const char* str) {
    while (*str) {
        uart_write_char(*str++); 
    }
}

int uart_read_line(char* buffer, int max_len) {
    int i = 0;
    uint8_t byte;

    while (i < max_len - 1) {
        if (fifo_get((Fifo_t *)&usart_rx_fifo, &byte) == 0) { 
            if (byte == '\r') continue; 
            if (byte == '\n') break;    
            buffer[i++] = byte;         
        }
    }
    buffer[i] = '\0'; 
    return i;         
}