#include "clock_.h"
#include "fifo.h"
#include "uart.h"
#include <string.h>
#include <stdio.h>

#define DEVICE_NAME "LEO"
#define FIELD_SZ 10 // Größe des Spielfelds

uint8_t opponent_field[FIELD_SZ][FIELD_SZ]; // Spielfeld des Gegners
uint8_t original_field[FIELD_SZ][FIELD_SZ]; // Originales Spielfeld
uint8_t next_shot_row = 0, next_shot_col = 0;


typedef enum {
    WAITING_START,
    WAITING_CS,
    MY_TURN,
    WAITING_FOR_RESPONSE,  // Neuer State
    OP_TURN,
    GAME_OVER
} GameState_t;

GameState_t current_state = WAITING_START; // Startzustand des Spiels

// Schiff-Definition
typedef struct {
    int row;        // Startzeile
    int col;        // Startspalte
    int length;     // Länge des Schiffs
    int horizontal; // 1 = horizontal, 0 = vertikal
} Ship_t;

// Schiffsliste nach den Regeln (Länge 5: 1x, Länge 4: 2x, Länge 3: 3x, Länge 2: 4x)
Ship_t ships[] = {
    // 1x Schiff der Länge 5
    {0, 0, 5, 1}, // Zeile 0, Spalte 0-4, horizontal

    // 2x Schiffe der Länge 4  
    {2, 2, 4, 0}, // Zeile 2-5, Spalte 2, vertikal
    {7, 0, 4, 1}, // Zeile 7, Spalte 0-3, horizontal

    // 3x Schiffe der Länge 3
    {1, 6, 3, 0}, // Zeile 1-3, Spalte 6, vertikal
    {5, 5, 3, 1}, // Zeile 5, Spalte 5-7, horizontal
    {7, 6, 3, 0}, // Zeile 7-9, Spalte 6, vertikal (korrigiert!)

    // 4x Schiffe der Länge 2
    {0, 7, 2, 1}, // Zeile 0, Spalte 7-8, horizontal
    {4, 0, 2, 0}, // Zeile 4-5, Spalte 0, vertikal (korrigiert!)
    {6, 9, 2, 0}, // Zeile 6-7, Spalte 9, vertikal
    {9, 3, 2, 1}  // Zeile 9, Spalte 3-4, horizontal
};
const int NUM_SHIPS = 10;





// Überprüft, ob eine Position auf dem Spielfeld gültig ist
int is_valid_position(int row, int col) {
    return (row >= 0 && row < FIELD_SZ && col >= 0 && col < FIELD_SZ);
}

// Überprüft, ob ein Schiff an einer Position platziert werden kann
int can_place_ship(uint8_t field[FIELD_SZ][FIELD_SZ], Ship_t ship) {
    for (int i = 0; i < ship.length; i++) {
        int row = ship.row + (ship.horizontal ? 0 : i);
        int col = ship.col + (ship.horizontal ? i : 0);
        
        if (!is_valid_position(row, col) || field[row][col] != 0) {
            return 0; // Kann nicht platziert werden
        }
    }
    return 1; // Kann platziert werden
}

// Platziert ein Schiff auf dem Spielfeld
void place_ship(uint8_t field[FIELD_SZ][FIELD_SZ], Ship_t ship) {
    for (int i = 0; i < ship.length; i++) {
        int row = ship.row + (ship.horizontal ? 0 : i);
        int col = ship.col + (ship.horizontal ? i : 0);
        
        if (is_valid_position(row, col)) {
            field[row][col] = 1;
        }
    }
}

// Spielfeld initialisieren
void init_field(uint8_t field[FIELD_SZ][FIELD_SZ]) {
    // Alle Felder auf 0 setzen (leer)
    memset(field, 0, FIELD_SZ * FIELD_SZ * sizeof(uint8_t));
    
    // Alle Schiffe platzieren
    for (int i = 0; i < NUM_SHIPS; i++) {
        if (can_place_ship(field, ships[i])) {
            place_ship(field, ships[i]);
        }
    }
}

// Checksumme berechnen
void calculate_checksum(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t checksum[FIELD_SZ]) {
    for (int row = 0; row < FIELD_SZ; row++) {
        checksum[row] = 0;
        for (int col = 0; col < FIELD_SZ; col++) {
            if (field[row][col] == 1) {
                checksum[row]++;
            }
        }
    }
}

// Checksumme senden
void send_checksum(uint8_t checksum[FIELD_SZ]) {
    uart_write_string("DH_CS_");

    for (int i = 0; i < FIELD_SZ; i++) {
        char digit = '0' + checksum[i];
        uart_write_char(digit);
    }

    uart_write_string("\n");
}

// Parst HD_BOOM_x_y Nachricht und extrahiert Koordinaten
int parse_boom_message(const char* buffer, int* row, int* col) {
    // Erwarte Format: "HD_BOOM_x_y" wo x und y einstellige Zahlen sind
    if (strlen(buffer) != 11) return 0; // Falsche Länge
    if (strncmp(buffer, "HD_BOOM_", 8) != 0) return 0; // Falsches Präfix
    
    // Extrahiere Koordinaten
    *row = buffer[8] - '0'; // x-Koordinate (Zeile)
    *col = buffer[10] - '0'; // y-Koordinate (Spalte)
    
    // Validiere Koordinaten
    if (*row < 0 || *row >= FIELD_SZ || *col < 0 || *col >= FIELD_SZ) {
        return 0; // Ungültige Koordinaten
    }
    
    return 1; // Erfolgreich geparst
}

int hit_count = 0;  // Globale Variable für die Anzahl der Treffer auf deinem Spielfeld

// Überprüft, ob ein Schuss ein Treffer ist und markiert das Feld
int process_shot(uint8_t field[FIELD_SZ][FIELD_SZ], int row, int col) {
    if (field[row][col] == 1) {  // Schiffsteil getroffen
        field[row][col] = 2;     // Markiere das Schiffsteil als getroffen
        hit_count++;             // Erhöhe die Trefferanzahl
        return 1;                // Treffer
    } else {
        if (field[row][col] == 0) {
            field[row][col] = 3; // Ins Wasser geschossen
        }
        return 0;                // Miss
    }
}

void get_next_shot(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t* row, uint8_t* col) {
    for (int r = 0; r < FIELD_SZ; r++) {
        for (int c = 0; c < FIELD_SZ; c++) {
            if (field[r][c] == 0) { // Nur leere Felder
                *row = r;
                *col = c;
                return;
            }
        }
    }
    *row = -1; // Keine gültigen Schüsse mehr
    *col = -1;
}

void send_shot(int row, int col) {
    uart_write_string("DH_BOOM_");
    uart_write_char('0' + row);  // ASCII-Konvertierung!
    uart_write_string("_");
    uart_write_char('0' + col);  // ASCII-Konvertierung!
    uart_write_string("\n");
}

void strategy_shot(uint8_t field[FIELD_SZ][FIELD_SZ]) {
    // Einfache Strategie: Nächster Schuss auf das nächste leere Feld
    get_next_shot(field, &next_shot_row, &next_shot_col);
    
    if (next_shot_row >= 0 && next_shot_col >= 0) {
        send_shot(next_shot_row, next_shot_col);
    } else {
        uart_write_string("DH_NO_MORE_SHOTS\n"); // Keine gültigen Schüsse mehr
    }
}

int all_ships_sunk(uint8_t field[FIELD_SZ][FIELD_SZ]) {
    for (int r = 0; r < FIELD_SZ; r++) {
        for (int c = 0; c < FIELD_SZ; c++) {
            if (field[r][c] == 1) {  // Noch nicht getroffenes Schiffsteil
                return 0;  // Es gibt noch Schiffsteile
            }
        }
    }
    return 1;  // Alle Schiffsteile sind getroffen
}

void send_game_over(uint8_t field[FIELD_SZ][FIELD_SZ]) {
    for (int r = 0; r < FIELD_SZ; r++) {
        uart_write_string("DH_SF");
        uart_write_char('0' + r);  // Zeilennummer
        uart_write_string("D");
        for (int c = 0; c < FIELD_SZ; c++) {
            uart_write_char('0' + original_field[r][c]);  // Originales Spielfeld
        }
        uart_write_string("\n");
    }
}

void initialize_game(uint8_t field[FIELD_SZ][FIELD_SZ]) {
    // Generiere das Spielfeld
    generate_field(field);

    // Kopiere das Spielfeld in original_field
    memcpy(original_field, field, sizeof(original_field));
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

    // Initialisiere das Spielfeld des Gegners
    memset(opponent_field, 0, sizeof(opponent_field)); // Setze das Spielfeld des Gegners auf leer

    while (1) {
        
        int len = uart_read_line_non_blocking(buffer, sizeof(buffer));

        switch (current_state) {
            case WAITING_START:
                if (len > 0 && strncmp(buffer, "HD_START", 8) == 0) {
                    uart_write_string("DH_START_");
                    uart_write_string(DEVICE_NAME);
                    uart_write_string("\n");
                    current_state = WAITING_CS;
                }
                break;

            case WAITING_CS:
                if (len > 0 && strncmp(buffer, "HD_CS_", 6) == 0) {
                    send_checksum(checksum);
                    current_state = OP_TURN;
                }
                break;

            case OP_TURN:
                if (len > 0 && strncmp(buffer, "HD_BOOM_", 8) == 0) {
                    int row, col;
                    if (parse_boom_message(buffer, &row, &col)) {
                        int is_hit = process_shot(field, row, col);
                        if (is_hit) {
                            uart_write_string("DH_BOOM_H\n");
                            hit_count++;  // Treffer zählen
                        } else {
                            uart_write_string("DH_BOOM_M\n");
                        }

                        // Prüfe, ob alle deine Schiffe versenkt wurden
                        if (hit_count >= 30) {
                            send_game_over(field);  // Sende das Spielfeld
                            current_state = GAME_OVER;  // Wechsel zu GAME_OVER
                        } else {
                            current_state = MY_TURN;  // Wechsel zu MY_TURN
                        }
                    }
                }
                break;

            case MY_TURN:
                strategy_shot(opponent_field);  // Schieße sofort
                current_state = WAITING_FOR_RESPONSE;  // Wechsel zu WAITING_FOR_RESPONSE
                break;

            case WAITING_FOR_RESPONSE:
                if (hit_count >= 30) {
                    send_game_over(field);  // Sende das Spielfeld
                    current_state = GAME_OVER;  // Wechsel zu GAME_OVER
                    uart_write_string("DH_SF_SENT\n");  // Debug-Ausgabe
                } else if (len > 0) {
                    if (strncmp(buffer, "HD_BOOM_H", 9) == 0) {
                        opponent_field[next_shot_row][next_shot_col] = 1;
                        current_state = OP_TURN;
                    } else if (strncmp(buffer, "HD_BOOM_M", 9) == 0) {
                        opponent_field[next_shot_row][next_shot_col] = 2;
                        current_state = OP_TURN;
                    } else if (strncmp(buffer, "HD_SF", 5) == 0) {
                        // Gegner hat sein Spielfeld gesendet, du hast gewonnen
                        current_state = GAME_OVER;
                    }
                }
                break;

            case GAME_OVER:
                if (all_ships_sunk(field)) {
                    // Sende dein Spielfeld
                    send_game_over(field);
                } else if (len > 0 && strncmp(buffer, "HD_SF", 5) == 0) {
                    // Gegner hat sein Spielfeld gesendet, du hast gewonnen
                    send_game_over(field);
                }
                break;
        }
    }

    return 0;
}