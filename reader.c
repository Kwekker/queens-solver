#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "reader.h"
#include "types.h"



// int readHaha(FILE *file, board_t *board) {
//     char c;
//     char *currentToken;
//     size_t tokenStart = -1;

//     while (1) {

//         while (isgraph(c = getc(file))) {
//             if (tokenStart == -1) tokenStart = ftell(file);

//         }

//         while (isprint());
//     }
// }


int readQueensFile(FILE *file, board_t *board) {

    const uint32_t size = board->size;

    // There are exactly the same amount of groups as there are queens.
    // The amount of queens is equal to the amount of rows (and cols).
    char identifiers[size];
    memset(identifiers, 0, size);
    uint32_t identifiers_index = 0;

    // Init the cellCounts to 0 because they start at -1.
    for (uint32_t i = 0; i < size; i++) {
        board->groups[i].cellCount = 0;
    }

    // Set all color and group values in the cells,
    // and set the cellCounts in the groups.
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            char c = getc(file);

            if (c == '\n') {
                fprintf(stderr, "Fuck uhhhh there's a newline in the middle of the board what???\n");
                return -1;
            }

            uint32_t color;

            // Check if we've seen this character before.
            char *identifiers_ptr = strchr(identifiers, c);
            if (identifiers_ptr != NULL) {
                color = identifiers_ptr - identifiers;
            }
            else {
                identifiers[identifiers_index] = c;
                color = identifiers_index;
                identifiers_index++;
            }

            cellSet_t *group = &board->groups[color];

            // Set the group values of the cell.
            board->cells[j + i * size].color = color;
            board->cells[j + i * size].group = group;
            group->cellCount++;
        }
        // Get rid of the pesky newline.
        getc(file);
    }

    // Allocate the cell pointer arrays in the group sets.
    for (uint32_t g = 0; g < size; g++) {
        cellSet_t *group = &board->groups[g];

        group->cells = malloc(group->cellCount * sizeof(cell_t*));

        // cellCount is used in the next for loop as a way to find
        // the index of the last unpopulated cell pointer.
        group->cellCount = 0;
    }

    // Populate the cell pointer arrays in the group sets.
    for (uint32_t c = 0; c < size * size; c++) {
        cell_t *cell = &board->cells[c];
        cellSet_t *group = &board->groups[cell->color];

        group->cells[group->cellCount] = cell;
        group->cellCount++;
    }

    return 0;
}


int measureQueensFile(FILE *file, uint32_t *size) {

    int read_char;
    uint32_t ret_width = 0;

    // Count the amount of characters in the first line.
    while ((read_char = fgetc(file)) != '\n' && read_char != EOF) {
        ret_width++;
    }

    if(read_char == EOF) {
        fprintf(stderr, "Your file is only one line????\n");
        return -1;
    }

    fseek(file, 0, SEEK_SET);

    uint32_t hori = 0;
    uint32_t vert = 1;

    // Count the lines
    while ((read_char = fgetc(file)) != EOF) {

        if (read_char == '\n') {
            // Make sure every line is the same length.
            if (hori != ret_width) {
                fprintf(stderr,
                    "%d is a malformed line :(\n Line is %d long, I expected %d 😔.\n",
                    vert, hori, ret_width
                );
                return -1;
            }

            if (hori == 0) {
                break;
            }

            vert++;
            hori = 0;
        }
        else {
            hori++;
        }
    }

    if (vert != ret_width) {
        fprintf(stderr,
            "Your file ain't square!!! That's not a solvable queens game!!\n"
            "I measured a width of %d and a height of %d 😔.\n",
            vert, ret_width
        );
    }

    *size = ret_width;

    return 0;
}