#include <unistd.h>
#include <stdio.h>

#include "clicker.h"
#include "looker.h"



void clickSolveBoard(
    board_t solution, boardScreenInfo_t screenInfo, uint32_t delay
) {

    initClicking();

    for (uint32_t s = 0; s < solution.size; s++) {
        cell_t *cell = solution.columns[s].cells[0];
        moveMouseTo(
            screenInfo.x + cell->x * screenInfo.offset,
            screenInfo.y + cell->y * screenInfo.offset
        );
        doubleClickMouse();
        printf("Clicking %d, %d\n",
            screenInfo.x + cell->x * screenInfo.offset,
            screenInfo.y + cell->y * screenInfo.offset
        );
        usleep(delay);
    }

    stopClicking();
}
