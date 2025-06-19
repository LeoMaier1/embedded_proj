#ifndef UART_H_
#define UART_H_

void uart_init(void);
void uart_write_string(const char* str);
void uart_write_char(int c);
int uart_read_line(char* buffer, int max_len);

#endif // UART_H_