#include <string.h>
#define DEVICE_NAME "LEO"





int main(void) {
    char buffer[32];

    // Initialize UART
    uart_init();

    while (1) {
        // Read incoming message
        int len = uart_read_line(buffer, sizeof(buffer));

        // Check if the message is "HD_START"
        if (len > 0 && strcmp(buffer, "HD_START") == 0) {
            // Respond with "DH_START_leo"
            uart_write_string("DH_START_");
            uart_write_string(DEVICE_NAME);
            uart_write_string("\n");
        }
        
    }

    return 0;
}