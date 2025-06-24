#include "clock_.h"
#include "fifo.h"
#include "uart.h"
#include <string.h>
#include <stdio.h>

#define DEVICE_NAME "LEO"
#define FIELD_SZ 10 // Größe des Spielfelds

#pragma region Global Variables

uint8_t opponent_field[FIELD_SZ][FIELD_SZ];         // Spielfeld des Gegners zum speichern von getroffenen oder verfehlten Schüssen
uint8_t original_field[FIELD_SZ][FIELD_SZ];         // Originales eigenes Spielfeld für die Ausgabe im Game over
uint8_t next_shot_row = 0, next_shot_col = 0;       // row und col für get_next_shot
int hit_count = 0;                                  // Check-Variable für getroffene Schiffe
int games_played = 0;                               // Anzahl der gespielten Spiele
int target_games = 100;                             // Anzahl der Spiele, die gespielt werden sollen
const int NUM_SHIPS = 10;                           // insgesammte Anzahl an Schiffen 

// typ aufzählung bekannter Konstanten für gamestate
typedef enum
{
    WAITING_START,
    WAITING_CS,
    MY_TURN,
    WAITING_FOR_RESPONSE,
    OP_TURN,
    GAME_OVER
} GameState_t;
GameState_t current_state = WAITING_START;          // Startzustand des Spiels

// struct Ship_t mit Daten für row col länge und ausrichtung
typedef struct
{
    int row;                                        // Startzeile
    int col;                                        // Startspalte
    int length;                                     // Länge des Schiffs
    int horizontal;                                 // 1 = horizontal, 0 = vertikal
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
#pragma endregion Global Variables


#pragma region Funktionen
int is_valid_position(int row, int col)
{
    return (row >= 0 && row < FIELD_SZ && col >= 0 && col < FIELD_SZ);
}

int can_place_ship(uint8_t field[FIELD_SZ][FIELD_SZ], Ship_t ship)
{
    // geht die Länge des Schiffs durch und schaut ob valid position ist und ob das Feld leer ist 
    for (int i = 0; i < ship.length; i++)
    {
        int row, col;
        
        if (ship.horizontal) {
            row = ship.row + 0;  // bei horizontalen Schiffen bleibt die Zeile gleich
            col = ship.col + i;  // bei horizontalen Schiffen erhöht sich die Spalte
        } else {
            row = ship.row + i;  
            col = ship.col + 0;  
        }

        if (!is_valid_position(row, col) || field[row][col] != 0)
        {
            return 0; // kann nicht platziert werden
        }
    }
    return 1; // kann platziert werden
}

void place_ship(uint8_t field[FIELD_SZ][FIELD_SZ], Ship_t ship)
{
    // geht länge des Schiffs durch und platziert es mit nummer der Länge im feld
    for (int i = 0; i < ship.length; i++)
    {
        int row, col;
        
        if (ship.horizontal) {
            row = ship.row;
            col = ship.col + i;
        } else {
            row = ship.row + i;
            col = ship.col;
        }
        field[row][col] = ship.length;
    }
}

void init_field(uint8_t field[FIELD_SZ][FIELD_SZ])
{
    //initialisiert das Spielfeld und platziert die Schiffe

    // alle Felder auf 0 setzen um fehler zu vermeiden und um das Spielfeld zu initialisieren
    // memset setzt alle Bytes im Array auf 0 (erste spalte ist das Array, zweite spalte ist der Wert der gesetzt wird und dritte spalte ist die Gröse des Arrays)
    memset(field, 0, FIELD_SZ * FIELD_SZ * sizeof(uint8_t));

    for (int i = 0; i < NUM_SHIPS; i++)
    {
        if (can_place_ship(field, ships[i]))    // check mit can_place_ship ob Koordinaten valid sind und ob Feld leer ist 
        {
            place_ship(field, ships[i]);        // platziere Schiff mit place_ship
        }
    }
}

void calculate_checksum(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t checksum[FIELD_SZ])
{
    // geht jede Zeile durch und addiert die Anzahl der Schiffsteile pro spalte
    for (int row = 0; row < FIELD_SZ; row++)
    {
        checksum[row] = 0;                          // initialisieren der Checksumme
        for (int col = 0; col < FIELD_SZ; col++)    // geht jede Spalte durch
        {
            if (field[row][col] > 0)                // wert in Feld größer als 0 => Schiffsteil
            {
                checksum[row]++;                    // addiert 1 zur Checksumme pro Schiffsteil
            }
        }
    }
}

void send_checksum(uint8_t checksum[FIELD_SZ])
{
    // sendet die Checksumme laut Protokoll
    uart_write_string("DH_CS_");

    for (int i = 0; i < FIELD_SZ; i++)      // geht jede Zeile durch und sendet jede checksumme pro Zeile nacheinander
    {
        char digit = '0' + checksum[i];     // '0' ist ascii wert 48 => addieren der checksummer ergibt asci code der Zahl in checksum[]
        uart_write_char(digit);             
    }

    uart_write_string("\n");                // Zeilenumbruch senden für ende der Nachricht 
}

int parse_boom_message(const char *buffer, int *row, int *col)
{
    // zieht die Koordinaten aus der Nachricht "HD_BOOM_x_y" heraus


    if (strlen(buffer) != 11)                           // Überprüfe die Länge der Nachricht
        return 0;
    if (strncmp(buffer, "HD_BOOM_", 8) != 0)            // schaut auf Prefix "HD_BOOM_"
        return 0;
    *row = buffer[8] - '0';                             // filtert x-Koordinate an stelle 8 in buffer heraus
    *col = buffer[10] - '0';                            // filter y-Koordinate an stelle 10 in buffer heraus

    // Validiere Koordinaten
    if (*row < 0 || *row >= FIELD_SZ || *col < 0 || *col >= FIELD_SZ)
    {
        return 0;
    }

    return 1;
}

int process_shot(uint8_t field[FIELD_SZ][FIELD_SZ], int row, int col)
{
    // kontrolliert ob boom vom Host ein HIT oder ein MISS war 
    // wenn feld an den Koordinaten > 0 ist => Schiffsteil => hit vom Host 
    if (field[row][col] > 0)
    {
        field[row][col] = 0;            // markiert Feld mit 0 (getroffenes Schiffsteil)
        hit_count++;                    // hitcount für kontrolle von ob game over
        return 1;                       // Hit
    }
    else                                // wenn MISS dan einfach 0 lassen rückgabewert 0
    {
        return 0; // Miss
    }
}

void get_next_shot(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t *row, uint8_t *col)
{
    // geht vorher den ganzen Rand des Spielfeldes durch und danach auf alle Spielfelder 
    // mit gerader Summe, folgend alle restlichen Spielfelder
    // verwenden Pointer da zwei Werte zurückgegeben werden müssen
    #pragma region Rand
    for (int c = 0; c < FIELD_SZ; c++)
    {
        if (field[0][c] == 0)
        {
            *row = 0;
            *col = c;
            return;
        }
    }

    for (int c = 0; c < FIELD_SZ; c++)
    {
        if (field[FIELD_SZ - 1][c] == 0)
        {
            *row = FIELD_SZ - 1;
            *col = c;
            return;
        }
    }

    for (int r = 0; r < FIELD_SZ; r++)
    {
        if (field[r][0] == 0)
        {
            *row = r;
            *col = 0;
            return;
        }
    }

    for (int r = 0; r < FIELD_SZ; r++)
    {
        if (field[r][FIELD_SZ - 1] == 0)
        {
            *row = r;
            *col = FIELD_SZ - 1;
            return;
        }
    }
    #pragma endregion Rand

    #pragma region Schachbrett
    for (int r = 0; r < FIELD_SZ; r++)
    {
        for (int c = 0; c < FIELD_SZ; c++)
        {
            if (field[r][c] == 0 && (r + c) % 2 == 0)
            {
                *row = r;
                *col = c;
                return;
            }
        }
    }

    for (int r = 0; r < FIELD_SZ; r++)
    {
        for (int c = 0; c < FIELD_SZ; c++)
        {
            if (field[r][c] == 0)
            {
                *row = r;
                *col = c;
                return;
            }
        }
    }
    #pragma endregion Schachbrett

    // fehlercode für wenn keine Schüsse mehr übrig sind
    *row = -1;
    *col = -1;
}

void send_shot(int row, int col)
{
    // sendet laut Protokoll boom vom Device -> Host
    // übergabewerte sind row und col auf welche geschossen werden will
    uart_write_string("DH_BOOM_");
    uart_write_char('0' + row);         // ASCII-Konvertierung
    uart_write_string("_");
    uart_write_char('0' + col);         // ASCII-Konvertierung
    uart_write_string("\n");
}

void strategy_shot(uint8_t field[FIELD_SZ][FIELD_SZ])
{
    // sendet Schuss mit send_shot auf Koordinaten welche in get_next_shot
    // ausgewählt werden 
    // &next_shot_x ist die adresse des int wo get_next_shot daten hinschiebt 
    get_next_shot(field, &next_shot_row, &next_shot_col);

    if (next_shot_row >= 0 && next_shot_col >= 0)
    {
        send_shot(next_shot_row, next_shot_col);
    }
}

void send_game_over(uint8_t field[FIELD_SZ][FIELD_SZ])
{
    // schickt die DH_SF nachricht mit dem originalen feld zeile für zeile
    for (int r = 0; r < FIELD_SZ; r++)                      // geht jede Zeile Durch
    {
        uart_write_string("DH_SF");
        uart_write_char('0' + r);
        uart_write_string("D");
        for (int c = 0; c < FIELD_SZ; c++)                  // geht jede Spalte durch
        {
            uart_write_char('0' + original_field[r][c]);    // schreibt jede Zahl der Spalte in der aktuellen Zeile
        }
        uart_write_string("\n");
    }
}

void reset_game(uint8_t field[FIELD_SZ][FIELD_SZ], uint8_t checksum[FIELD_SZ])
{
    // reseten des Spiels für das Turnament
    init_field(field);                                      // initialisiert das Feld

    memcpy(original_field, field, sizeof(original_field));  // kopiert das Spielfeld "field" in das Array "original_field" für die SF-Ausgabe

    calculate_checksum(field, checksum);                    // berechnet die neue Checksum

    memset(opponent_field, 0, sizeof(opponent_field));      // Setze das Spielfeld des Gegners auf leer

    hit_count = 0;                                          // setze hit_count zurück
    next_shot_row = 0;                                      // setze die nächste Schussposition zurück
    next_shot_col = 0;                                      // setze die nächste Schussposition zurück
    current_state = WAITING_START;                          // setze den Zustand zurück auf WAITING_START
}
#pragma endregion Funktionen

int main(void)
{
    uint8_t field[FIELD_SZ][FIELD_SZ];  // Spielfeld
    uint8_t checksum[FIELD_SZ];         // Checksumme für jede Zeile
    char buffer[32];                    // Puffer für empfangene Nachrichten

    // initialisiere UART
    uart_init();

    reset_game(field, checksum);        // führt alle initialisierungen durch

    while (1)
    {
        // länge der auf der uart empfangenen Nachricht
        int len = uart_read_line_non_blocking(buffer, sizeof(buffer));

        // State-Machine des Spiels
        switch (current_state)
        {
        #pragma region WAITING_START
        case WAITING_START:
            // schickt als antwort auf HD_START_ -> DH_START_ mit meinem Namen am Ende
            // wechselt anschließend in den state WAITING_CS
            if (len > 0 && strncmp(buffer, "HD_START", 8) == 0)
            {
                uart_write_string("DH_START_");
                uart_write_string(DEVICE_NAME);
                uart_write_string("\n");
                current_state = WAITING_CS;
            }
            break;
        #pragma endregion WAITING_START

        #pragma region WAITING_CS
        case WAITING_CS:
            // wartet auf die Checksumme des Hosts und anwortet direkt mit der eigenen
            // wechselt anschließend in den state OP_TURN
            if (len > 0 && strncmp(buffer, "HD_CS_", 6) == 0)
            {
                send_checksum(checksum);
                current_state = OP_TURN;       
            }
            break;
        #pragma endregion WAITING_CS
        
        #pragma region OP_TURN
        case OP_TURN:
            // ruft die parse_boom_message auf und bekommt die Koordinaten des Schusses zurück
            // mit process_shot wird gecheckt ob es ein hit ist
            if (len > 0 && strncmp(buffer, "HD_BOOM_", 8) == 0)
            {
                int row, col;
                if (parse_boom_message(buffer, &row, &col))             // parsed die Koordinaten 
                {
                    int is_hit = process_shot(field, row, col);         // checked ob es ein Hit war
                    if (is_hit)
                    {
                        if (hit_count == 30)                            // checkt für Spielende ob Anzahl an maximalen Hits erreicht ist  
                        {
                            current_state = GAME_OVER;                  // wenn ja wechselt zu GAME_OVER
                        }
                        else
                        {
                            uart_write_string("DH_BOOM_H\n");           // Hit senden
                        }
                    }
                    else
                    {
                        uart_write_string("DH_BOOM_M\n");               // Miss senden
                    }
                    current_state = MY_TURN;                            // wechselt zu MY_TURN
                }
            }
            break;
        #pragma endregion OP_TURN

        #pragma region MY_TURN
        case MY_TURN:
            // schiest direkt mittels strategy_shot zurück und wechselt dann zu WAITING_FOr_RESPONSE
            strategy_shot(opponent_field);
            current_state = WAITING_FOR_RESPONSE;       // Wechsel zu WAITING_FOR_RESPONSE
            break;
        #pragma endregion MY_TURN

        #pragma region WAITING_FOR_RESPONSE
        case WAITING_FOR_RESPONSE:
            // checkt die UART für antwort vom Host auf den Schuss und markiert Hit oder Miss im opponent_field
            if (len > 0)
            {
                if (strncmp(buffer, "HD_BOOM_H", 9) == 0)
                {
                    opponent_field[next_shot_row][next_shot_col] = 1;           // markiert opponent_field mit 1 für Mit 
                    current_state = OP_TURN;
                }
                else if (strncmp(buffer, "HD_BOOM_M", 9) == 0)
                {
                    opponent_field[next_shot_row][next_shot_col] = 2;           // markiert opponent_field mit 2 für Miss
                    current_state = OP_TURN;
                }
                else if (strncmp(buffer, "HD_SF", 5) == 0)
                {
                    // wenn Gegner keine schüsse hat sendet HD_SF -> GAME_OVER
                    current_state = GAME_OVER;
                }
            }
            break;
            #pragma endregion WAITING_FOR_RESPONSE

        #pragma region GAME_OVER
        case GAME_OVER:
            // geht durch die Game over prozedur durch
            if (hit_count == 30)                                    // checkt ob man verloren hat 
            {
                send_game_over(field);                              // sendet eigenes Spielfeld mit dem Präfix SF
                games_played++;                                     // zählt gespielte Spiele hoch

                if (games_played < target_games)                    // checkt ob für turnament anzahl an spiele erreicht wurde
                {
                    reset_game(field, checksum);                    // führt den Reset des Spiels aus 
                }
            }
            else if (len > 0 && strncmp(buffer, "HD_SF", 5) == 0)   // checkt ob man gewonnen hat (Gegner schickt sein Spielfeld)
            {
                games_played++;                                     // zählt gespielte Spiele hoch
                send_game_over(field);
                if (games_played < target_games)                    // checkt ob für turnament anzahl an spiele erreicht wurde
                {
                    reset_game(field, checksum);                    // führt den Reset des Spiels aus 
                }
            }
            break;
            #pragma endregion GAME_OVER
        }
    }

    return 0;
}