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
int hit_count = 0;
int games_played = 0; // Anzahl der gespielten Spiele
int games_won = 0;    // Anzahl der gewonnenen Spiele
int target_games = 100; // Anzahl der Spiele, die gespielt werden sollen

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
    {0, 0, 5, 1}, // Zeile 0, Spalte 0, horizontal

    // 2x Schiffe der Länge 4  
    {2, 0, 4, 0}, // Zeile 2, Spalte 0, vertikal
    {1, 9, 4, 0}, // Zeile 1, Spalte 9, vertikal

    // 3x Schiffe der Länge 3
    {5, 3, 3, 1}, // Zeile 5, Spalte 3, horizontal
    {2, 7, 3, 0}, // Zeile 2, Spalte 7, vertikal
    {7, 5, 3, 1}, // Zeile 7, Spalte 5, horizontal

    // 4x Schiffe der Länge 2
    {0, 6, 2, 1}, // Zeile 0, Spalte 6, horizontal
    {7, 0, 2, 0}, // Zeile 7, Spalte 0, vertikal
    {6, 9, 2, 0}, // Zeile 6, Spalte 9, vertikal
    {9, 5, 2, 1}  // Zeile 9, Spalte 5, horizontal
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
            field[row][col] = ship.length;  // Markiere mit der Länge des Schiffs
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
            if (field[row][col] > 0) {  // Zähle jedes Schiffsteil (unabhängig vom Wert)
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



// Überprüft, ob ein Schuss ein Treffer ist und markiert das Feld
int process_shot(uint8_t field[FIELD_SZ][FIELD_SZ], int row, int col) {
    if (field[row][col] > 0) {  // feld mit schiffsteil
        field[row][col] = 0;    // markiere mit 0 (getroffenes Schiffsteil)
        hit_count++;            // hitcount für kontrolle von ob game over
        return 1;               // Hit
    } else {
        if (field[row][col] == 0) {
            field[row][col] = 3; // Ins Wasser geschossen
        }
        return 0;                // Miss
    }
}

void get_next_shot(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t* row, uint8_t* col){
    for (int r = 0; r < FIELD_SZ; r++) {
        for (int c = 0; c < FIELD_SZ; c++) {
            if (field[r][c] == 0 && (r + c) % 2 == 0) { // diagonale (alle felder mit gerader Summe)
                *row = r;
                *col = c;
                return;
            }
        }
    }

    for(int r = 0; r < FIELD_SZ; r++) {
        for (int c = 0; c < FIELD_SZ; c++) {
            if (field[r][c] == 0) { // Nächstes leeres Feld
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

void reset_game(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t checksum[FIELD_SZ]) {
    // Reset des Spielfelds

    init_field(field);

    memcpy(original_field, field, sizeof(original_field));
    
    calculate_checksum(field, checksum);

    memset(opponent_field, 0, sizeof(opponent_field)); // Setze das Spielfeld des Gegners auf leer

    hit_count = 0; // Setze hit_count zurück
    next_shot_row = 0; // Setze die nächste Schussposition zurück
    next_shot_col = 0; // Setze die nächste Schussposition zurück
    current_state = WAITING_START; // Setze den Zustand zurück
}






int main(void) {
    uint8_t field[FIELD_SZ][FIELD_SZ]; // Spielfeld
    uint8_t checksum[FIELD_SZ];        // Checksumme für jede Zeile
    char buffer[32];                   // Puffer für empfangene Nachrichten

    // Initialisiere UART
    uart_init();

    // Spielfeld initialisieren
    init_field(field);

    // Kopiere das Spielfeld in original_field
    memcpy(original_field, field, sizeof(original_field));

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
                            if(hit_count == 30) {
                                current_state = GAME_OVER;  // Wechsel zu GAME_OVER
                            }else {
                                uart_write_string("DH_BOOM_H\n");
                            }
                        } else {
                            uart_write_string("DH_BOOM_M\n");
                        }

                        // Prüfe, ob alle deine Schiffe versenkt wurden
                        if (hit_count == 30) {
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
                if (len > 0) {
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
                if (hit_count == 30) {
                    // Sende dein Spielfeld
                    send_game_over(field);
                    games_played++;

                    if (games_played < target_games) {
                        reset_game(field, checksum); // Setze das Spiel zurück
                    }
                } else if (len > 0 && strncmp(buffer, "HD_SF", 5) == 0) {
                    // Gegner hat sein Spielfeld gesendet, du hast gewonnen
                    games_played++;
                    games_won++;
                    send_game_over(field);
                    if (games_played < target_games) {
                        reset_game(field, checksum); // Setze das Spiel zurück
                    }
                }
                break;
        }
    }

    return 0;
}