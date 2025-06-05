#include <stm32f0xx.h>
#include "clock_.h"
#include <stdio.h>
void uart_init(void)
{
    EPL_SystemClock_Config();
    const uint8_t UART2_TX_PIN = 2; // PA2
    const uint8_t UART2_RX_PIN = 3; // PA3

    RCC->AHBENR |= RCC_AHBENR_GPIOAEN; // Enable GPIOA clock
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN; // Enable USART2 clock

    GPIOA->MODER &= ~(0b11 << (UART2_TX_PIN * 2) | 0b11 << (UART2_RX_PIN * 2)); // Clear PA2 and PA3 mode bits
    GPIOA->MODER |= (0b10 << (UART2_TX_PIN * 2)) | (0b10 << (UART2_RX_PIN * 2)); // Set PA2 and PA3 to alternate function mode
    
    
    GPIOA->AFR[0] &= ~(0b1111 << (UART2_TX_PIN * 4) | 0b1111 << (UART2_RX_PIN * 4)); // Clear PA2 and PA3 alternate function bits
    GPIOA->AFR[0] |= (0b0001 << (UART2_TX_PIN * 4)) | (0b0001 << (UART2_RX_PIN * 4)); // Set PA2 and PA3 to AF1 (USART2)


    USART2->BRR = (APB_FREQ / 115200); // Set baud rate to 9600

    USART2->CR1 |= (0b1 << 2 | 0b1 << 3 | 0b1 << 0); // Enable USART2, TX, and RX
}

void uart_write_char(int c)
{
    while (!(USART2->ISR & USART_ISR_TXE)); // Wait until TXE (Transmit Data Register Empty) is set
    USART2->TDR = c; // Write character to transmit data register
}

void uart_write_string(const char* str)
{;
    while(*str)
    {
        uart_write_char(*str++); // Write each character in the string
    }
}

char uart_read_char(void)
{
    while (!(USART2->ISR & USART_ISR_RXNE)); // Wait until RXNE (Read Data Register Not Empty) is set
    return USART2->RDR; // Read character from receive data register
}

int uart_read_line(char* buffer, int max_len)
{
    int i = 0;
    char c;

    while(i < max_len -1)
    {
        c = uart_read_char(); // Read character from UART
        if(c == '\r') continue; // Ignore carriage return
        if(c == '\n') break; // Stop reading on newline
        buffer[i++] = c; // Store character in buffer
    }
    buffer[i] = '\0'; // Null-terminate the string
    return i; // Return the number of characters read
}


