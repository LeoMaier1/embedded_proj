#include <stm32f0xx.h>
#include "clock_.h"
#include "fifo.h"

// Select the Baudrate for the UART
#define BAUDRATE 115200

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
    while (!(USART2->ISR & USART_ISR_TXE));                     // wartet bis uart sender bereit ist 
    USART2->TDR = c;                                            // schreibt in TDR Register Daten 
}

void uart_write_string(const char* str) {                       // array wird als Pointer übergeben
    while (*str) {                                              // geht array durch und sendet jedes zeichen mittel uart_write_char
        uart_write_char(*str++); 
    }
}
/*
int uart_read_line(char* buffer, int max_len) {                 // pointer weil empfangener String mus zurückgegeben werden und mit array geht das nicht
    int i = 0;
    uint8_t byte;

    while (i < max_len - 1) {                                   // schleife läuft bis ende der Zeile erreicht          
        if (fifo_get((Fifo_t *)&usart_rx_fifo, &byte) == 0) {   // fifo get liest byte aus FIFO braucht adresse von fifo und von data beides muss zurückgegeben werden 
            if (byte == '\r') continue;                         
            if (byte == '\n') break;                            // nachricht ist fertig
            buffer[i++] = byte;                                 // schreibt in buffer Array die daten aus dem fifo
        }
    }
    buffer[i] = '\0';                                           // nullterminierung hinzufügen
    return i;                                                   // länge des strings returnen
}
*/


//CHAT
int uart_read_line_non_blocking(char* buffer, int max_len) {
    static int index = 0;  // Speichert, wie viele Zeichen bisher gelesen wurden
    uint8_t byte;          // Temporäre Variable für das gelesene Byte

    // Prüfe, ob Daten in der FIFO verfügbar sind
    if (fifo_get((Fifo_t *)&usart_rx_fifo, &byte) == 0) {  
        // FIFO liefert ein Byte (kein Fehler)

        if (byte == '\r') {
            // Ignoriere carriage return (ASCII 13)
            return 0;  // Noch keine vollständige Nachricht
        }

        if (byte == '\n') {
            // Nachricht abgeschlossen (newline-Zeichen empfangen)
            buffer[index] = '\0';  // Null-terminiere den String
            int len = index;       // Speichere die Länge der Nachricht
            index = 0;             // Setze den Index zurück für die nächste Nachricht
            return len;            // Gib die Länge der Nachricht zurück
        }

        // Füge das Byte zum Puffer hinzu
        if (index < max_len - 1) {
            buffer[index++] = byte;  // Speichere das Byte im Puffer
        }
    }

    return 0;  // Keine vollständige Nachricht empfangen
}