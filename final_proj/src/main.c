#include "clock_.h"
#include "fifo.h"
#include "uart.h"
#include <string.h>
#include <stdio.h>

#define DEVICE_NAME "LEO"
#define FIELD_SZ 10 // Größe des Spielfelds

// Spielfeld initialisieren
void init_field(uint8_t field[FIELD_SZ][FIELD_SZ]) {
    // Alle Felder auf 0 setzen (leer)
    memset(field, 0, FIELD_SZ * FIELD_SZ);

    // Beispiel: Manuelles Platzieren von Schiffen
    // Schiff der Länge 5 (horizontal)
    field[0][0] = 1; field[0][1] = 1; field[0][2] = 1; field[0][3] = 1; field[0][4] = 1;

    // Schiff der Länge 4 (vertikal)
    field[2][2] = 1; field[3][2] = 1; field[4][2] = 1; field[5][2] = 1;
}

// Checksumme berechnen
void calculate_checksum(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t checksum[FIELD_SZ]) {
    for (int row = 0; row < FIELD_SZ; row++) {
        checksum[row] = 0; // Initialisiere die Checksumme für die Zeile
        for (int col = 0; col < FIELD_SZ; col++) {
            if (field[row][col] == 1) {
                checksum[row]++; // Zähle Schiffsteile in der Zeile
            }
        }
    }
}

// Checksumme senden
void send_checksum(uint8_t checksum[FIELD_SZ]) {
    char message[32];
    uart_write_string("DH_CS_"); // Präfix senden

    for (int i = 0; i < FIELD_SZ; i++) {
        sprintf(message, "%d", checksum[i]); // Checksumme in Text umwandeln
        uart_write_string(message);         // Checksumme senden
    }

    uart_write_string("\n"); // Zeilenumbruch senden
}

int main(void) {
    uint8_t field[FIELD_SZ][FIELD_SZ]; // Spielfeld
    uint8_t checksum[FIELD_SZ];        // Checksumme für jede Zeile
    char buffer[32];                   // Puffer für empfangene Nachrichten

    // Initialisiere UART
    uart_init();

    // Spielfeld initialisieren
    init_field(field);

    // Checksumme berechnen
    calculate_checksum(field, checksum);

    while (1) {
        // Lese eine Nachricht aus der UART
        int len = uart_read_line(buffer, sizeof(buffer));

        // Prüfen der Nachricht "HD_START"
        if (len > 0 && strcmp(buffer, "HD_START") == 0) {
            // Sende der Antwort "DH_START_LEO"
            uart_write_string("DH_START_");
            uart_write_string(DEVICE_NAME);
            uart_write_string("\n");

            // Senden der Checksumme
            send_checksum(checksum);
        }
    }

    return 0;
}