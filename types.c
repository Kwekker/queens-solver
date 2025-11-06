#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "debug_prints.h"

void freeBoard(board_t board);


board_t createBoard(uint32_t size) {
    board_t ret = {0};

    ret.size = size;
    ret.cells = calloc(size * size, sizeof(cell_t));

    cellSet_t *sets = calloc(size * 3, sizeof(cellSet_t));

    // Make sure the three cellSet arrays are consecutive.
    ret.set_arrays[0] = sets;
    ret.set_arrays[1] = sets + size;
    ret.set_arrays[2] = sets + 2 * size;

    // Populate the set arrays.
    for (uint32_t i = 0; i < size; i++) {

        // Populate the columns with cells.
        ret.columns[i].cells = calloc(size, sizeof(cell_t*));
        ret.columns[i].cellCount = size;
        ret.columns[i].identifier = i;
        for (uint32_t j = 0; j < size; j++) {
            ret.columns[i].cells[j] = &ret.cells[j * size + i];
        }

        // Populate the rows with cells.
        ret.rows[i].cells = calloc(size, sizeof(cell_t*));
        ret.rows[i].cellCount = size;
        ret.rows[i].identifier = i;
        for (uint32_t j = 0; j < size; j++) {
            ret.rows[i].cells[j] = &ret.cells[i * size + j];
        }

        // We do not yet know the size of the groups,
        // so we cannot populate the groups set.
        // However, we need to be able to differentiate between an
        // _unset_ group and an _empty_ group, because the former
        // doesn't have any allocated cell arrays where the latter does.
        // Thus, we set its count to -1.
        ret.groups[i].cellCount = -1;
        ret.groups[i].identifier = i;
    }


    // Populate the cell array.
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            cell_t *cell = &ret.cells[j * size + i];
            cell->x = i;
            cell->y = j;
            cell->column = &ret.columns[i];
            cell->row = &ret.rows[j];
        }
    }

    return ret;
}

// Fucking whole function for freeing one of these beasts.
// I'm starting to understand the whole c++ unique_ptr and shared_ptr business.
// Still a shitty language though.
void freeBoard(board_t board) {
    for (uint8_t s = 0; s < 3; s++) {

        for (uint32_t i = 0; i < board.size; i++) {
            if (board.set_arrays[s][i].cellCount != -1) {
                // Free the array of pointers in the set array.
                free(board.set_arrays[s][i].cells);
            }
        }

    }

    // Free the set arrays.
    free(board.set_arrays[0]);

    // Free the cells.
    free(board.cells);
    // Cells don't contain any heap pointers so they don't need special care.

}

// Returns a copy of the input board, with new arrays.
// The returned board should be freed using freeBoard when you are done with it.
board_t copyBoard(board_t board) {
    const uint32_t size = board.size;

    board_t copy = createBoard(size);
    for (uint32_t c = 0; c < size * size; c++) {
        copy.cells[c].color = board.cells[c].color;
        copy.cells[c].type = board.cells[c].type;
        copy.cells[c].group = &copy.groups[copy.cells[c].color];
    }


    for (uint32_t i = 0; i < size; i++) {
        copy.groups[i].cells = malloc(
            board.groups[i].cellCount * sizeof(cell_t*)
        );
    }

    for (uint32_t s = 0; s < 3; s++) {
        for (uint32_t i = 0; i < size; i++) {
            cellSet_t *set = &copy.set_arrays[s][i];
            set->cellCount = board.set_arrays[s][i].cellCount;
            for (uint32_t c = 0; c < set->cellCount; c++) {
                cell_t *bCell = board.set_arrays[s][i].cells[c];
                set->cells[c] = &copy.cells[bCell->x + bCell->y * size];
            }
            copy.set_arrays[s][i].solved = board.set_arrays[s][i].solved;
        }
    }

    return copy;
}


// Moves all populated pointers to the left,
// and sets the cellCount accordingly.
// _ 1 _ _ 2 3 _ 4 5 6 _ 7
// v
// 1 2 3 4 5 6 7
void cleanSet(cellSet_t *set) {
    uint32_t empty_i = 0;
    for (uint32_t i = 0; i < set->cellCount; i++) {
        if (set->cells[i] != NULL) {
            set->cells[empty_i] = set->cells[i];
            empty_i++;
        }
    }
    set->cellCount = empty_i;
}

// Returns the 4 diagonally adjacent cells to a cell.
corners_t getCorners(board_t board, cell_t cell) {
    corners_t corners = {0};

    uint8_t corner_index = 0;
    int8_t x[4] = {-1, 1, -1, 1};
    int8_t y[4] = {-1, -1, 1, 1};

    for (uint8_t i = 0; i < 4; i++) {
        // Check if the corner is in bounds.
        int32_t newX = cell.x + x[i];
        int32_t newY = cell.y + y[i];

        // This has to be a pointer because its index could be
        // outside the bounds of the board's cells array.
        // If it wasn't a pointer, C would try to copy the contents
        // of the array entry into this variable, which could result
        // in a segfault.
        cell_t *corner = &board.cells[newY * board.size + newX];

        if ( // Thank you short-circuiting, my hero.
            newX < 0 || newX >= board.size
            || newY < 0 || newY >= board.size
            || corner->type == CELL_CROSSED
        ) continue;

        // Having it be a pointer is also nice for this tbh.
        corners.cells[corner_index++] = corner;
    }

    corners.count = corner_index;

    return corners;
}


void colorBoard(board_t board, uint32_t *colors) {
    // Populate group and color fields in the cells,
    // and count the cells for every group.
    for (uint32_t i = 0; i < board.size * board.size; i++) {
        board.groups[colors[i]].cellCount++;
        board.cells[i].color = colors[i];
        board.cells[i].group = &board.groups[colors[i]];
    }

    // Allocate the group cell pointer arrays.
    for (uint32_t i = 0; i < board.size; i++) {
        cellSet_t *group = &board.groups[i];
        group->cells = malloc((group->cellCount + 1) * sizeof(cell_t*));
        group->cellCount = 0;

    }

    // Populate the group cell pointer arrays.
    for (uint32_t i = 0; i < board.size * board.size; i++) {
        cellSet_t *group = &board.groups[colors[i]];
        group->cells[group->cellCount] = &board.cells[i];
        group->cellCount++;
    }

}

#undef DEBUG_PRINT_MODE
#define DEBUG_PRINT_MODE PRINT_CROSSINGS

void crossCell(cell_t *cell) {
    DPRINTF("Crossing cell [\x1b[90m%d, %d\x1b[0m]\n", cell->x, cell->y);

    cell->type = CELL_CROSSED;

    for (uint8_t s = 0; s < 3; s++) {
        cellSet_t *set = cell->sets[s];

        for (uint32_t c = 0; c < set->cellCount; c++) {
            if (set->cells[c] == cell) {
                // Move the last cell of the set into
                // the newly created hole.
                set->cells[c] = set->cells[set->cellCount - 1];
                set->cellCount--;
                // TODO: Check for queens!
                break;
            }
        }
    }
}


#undef DEBUG_PRINT_MODE
#define DEBUG_PRINT_MODE PRINT_NORMAL


uint8_t inSet(cellSet_t *set, cell_t* cell) {
    return set == cell->column
        || set == cell->row
        || set == cell->group;
}


uint8_t checkBoard(board_t board) {
    for (uint32_t i = 0; i < board.size; i++) {
        if (
            board.columns[i].variable
            || board.rows[i].variable
            || board.groups[i].variable
        ) return -1;
    }

    return 0;
}


uint8_t checkSets(board_t board) {
    for (uint32_t i = 0; i < board.size; i++) {
        if (
            board.columns[i].cellCount <= 0
            || board.rows[i].cellCount <= 0
            || board.groups[i].cellCount <= 0
        ) return -1;
    }

    return 0;
}


void printBoard(board_t board, uint32_t indentation) {

    if (board.size == 0) {
        fprintf(stderr, "Board has size 0??? I can't print that!!\n");
    }

    for (uint8_t t = 0; t < indentation; t++) printf("\t");
    printf("   ");
    for (uint32_t i = 0; i < board.size; i++) {
        printf("%2d", i);
    }
    printf("\n");

    char typeChars[] = {'o', '.', 'Q', 'M'};
    uint32_t textColors[] = {
        31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97
    };

    for (uint32_t j = 0; j < board.size; j++) {
        for (uint8_t t = 0; t < indentation; t++) printf("\t");
        printf("\x1b[%dm%2d ", textColors[j], j);
        for (uint32_t i = 0; i < board.size; i++) {
            if (board.cells[j * board.size + i].type > 3) {
                printf("\x1b[%dm E",
                    textColors[board.cells[j * board.size + i].color]
                );
            }
            else {
                printf("\x1b[%dm %c",
                    textColors[board.cells[j * board.size + i].color],
                    typeChars[board.cells[j * board.size + i].type]
                );
            }
        }
        printf("\n\x1b[0m");
    }

}


void printBoardVars(board_t board) {

    printf("   ");
    for (uint32_t i = 0; i < board.size; i++) {
        printf("%2d", i);
    }
    printf("\n");

    uint32_t textColors[] = {
        31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97
    };

    for (uint32_t j = 0; j < board.size; j++) {
        printf("\x1b[%dm%2d ", textColors[j], j);
        for (uint32_t i = 0; i < board.size; i++) {
            if (board.cells[j * board.size + i].type == CELL_CROSSED) {
                printf("\x1b[%dm .",
                    textColors[board.cells[j * board.size + i].color]
                );
            }
            else {
                printf("\x1b[%dm %d",
                    textColors[board.cells[j * board.size + i].color],
                    board.cells[j * board.size + i].variable
                );
            }
        }
        printf("\n\x1b[0m");
    }

}


void visuPrompt(board_t board, cell_t *blockCell, cell_t *markCell, cellSet_t *markSet) {

    printf("   ");
    for (uint32_t i = 0; i < board.size; i++) {
        printf("%2d", i);
    }
    printf("\n");

    char typeChars[] = {'o', '.', 'Q', 'M'};

    for (uint32_t j = 0; j < board.size; j++) {
        printf("%2d ", j);
        for (uint32_t i = 0; i < board.size; i++) {
            cell_t *cell = &board.cells[j * board.size + i];

            uint32_t textColor = 90;
            if (cell == blockCell) textColor = 31;
            else if (cell == markCell) textColor = 33;
            else if (inSet(markSet, cell)) textColor = 34;

            if (cell->type >= sizeof(typeChars)) {
                printf("\x1b[%dm E", textColor);
            }
            else {
                printf("\x1b[%dm %c", textColor, typeChars[cell->type]);
            }
        }
        printf("\n\x1b[0m");
    }

    printf(
        "Cell [\x1b[31m%d, %d\x1b[0m] is blocking because of cell "
        "[\x1b[33m%d, %d\x1b[0m]'s set \x1b[34m%d\x1b[0m. ",
        blockCell->x, blockCell->y,
        markCell->x, markCell->y, markSet->identifier
    );
}