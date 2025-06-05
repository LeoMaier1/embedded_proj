#include <stdio.h>
#include <stdlib.h>

#define GRID_SIZE 10
#define WATER 0
#define CARRIER 5
#define BATTLESHIP 4
#define CRUISER 3
#define SUBMARINE 3
#define DESTROYER 2

int grid[GRID_SIZE][GRID_SIZE];

typedef struct{
    int x;
    int y;
}Position;

typedef struct {
    int size;
    int id;
}Ship;

static Ship ships[] = {
    {CARRIER, 1},
    {BATTLESHIP, 2},
    {CRUISER, 3},
    {SUBMARINE, 4},
    {DESTROYER, 5}
};



void grid_init() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = WATER;
        }
    }
}

void set_ship_manually()
{
    for(int i = 0; i < CARRIER; i++)
    {
        grid[0][i] = 1;
    }

    for(int i = 0; i < BATTLESHIP; i++)
    {
        grid[2][3+i] = 2;
    }

    for(int i = 0; i < CRUISER; i++)
    {
        grid[5+i][5] = 3;
    }

    for(int i = 0; i < SUBMARINE; i++)
    {
        grid[8][1+i] = 4;
    }

    for(int i = 0; i < DESTROYER; i++)
    {
        grid[0+i][9] = 5;
    }
}

