#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "seeer.h"
#include "looker.h"
#include "debug_prints.h"

#define MIN(i, j) (((i) < (j)) ? (i) : (j))
#define MAX(i, j) (((i) > (j)) ? (i) : (j))


// Very arbitrary value. Should be more than 3,
// should never come close to 100.
#define BIN_MARGIN 12


#define BLACK (pixel_t){.r = 0, .g = 0, .b = 0}
#define WHITE (pixel_t){.r = 255, .g = 255, .b = 255}


typedef struct {
    int32_t coordinate;
    uint32_t count;
} bin_t;


static inline pixel_t* getPix(image_t img, uint32_t x, uint32_t y);
static uint8_t isCrossing(image_t img, pixel_t *pix, int pixelOffset);
static uint32_t getPoints(image_t img, coord_t **points, int pixelOffset);
static inline uint16_t sum(pixel_t pixel);
static inline uint8_t isBlack(pixel_t pixel);

static uint32_t *findColors(image_t img, bin_t *xBins, bin_t *yBins, uint32_t size);
static coord_t getBins(
    coord_t *points, uint32_t pointCount, bin_t *xBins, bin_t *yBins
);
static void sanitizeBins(
    bin_t *xBins, uint32_t *xBins_i, bin_t *yBins, uint32_t *yBins_i
);


#undef DEBUG_PRINT_MODE
#define DEBUG_PRINT_MODE PRINT_SEEING


int compare_bins(const void *a_ptr, const void *b_ptr) {
    const bin_t a = *((bin_t *) a_ptr);
    const bin_t b = *((bin_t *) b_ptr);

    if (a.coordinate < b.coordinate) return -1;
    if (a.coordinate > b.coordinate) return 1;
    return 0;
}


uint32_t detectBoard(
    image_t img, uint32_t **board, boardScreenInfo_t *screenInfo, uint32_t crossingOffset
) {

    DPRINTF("Getting points :)\n");
    coord_t *points;
    uint32_t pointCount = getPoints(img, &points, crossingOffset);

    // We need at least a 5x5 board  (4x4 Queens is impossible).
    if (pointCount < 16) {
        free(points);
        printf("No board detected, or the board is too small.\n");
        return 0;
    }

    DPRINTF("Found %u points!!!\n", pointCount);

    // Put the point coordinates into bins.
    bin_t *xBins = malloc(pointCount * sizeof(bin_t));
    bin_t *yBins = malloc(pointCount * sizeof(bin_t));

    printf("Getting bins.\n");
    coord_t binCounts = getBins(points, pointCount, xBins, yBins);
    uint32_t xBins_n = binCounts.x;
    uint32_t yBins_n = binCounts.y;

    printf("xBins: ");
    for (uint32_t i = 0; i < xBins_n; i++) {
        printf("[%d: %d] ", xBins[i].coordinate, xBins[i].count);

    }
    printf("\nyBins: ");
    for (uint32_t i = 0; i < yBins_n; i++) {
        printf("[%d: %d] ", yBins[i].coordinate, yBins[i].count);

    }
    printf("\n");

    // xBins_n is the amount of crossings. We are looking for cells,
    // which is the amount of crossings + 1.
    uint32_t size = xBins_n + 1;

    *board = findColors(img, xBins, yBins, size);

    if (*board == NULL) {
        free(points);
        free(xBins);
        free(yBins);
        return 0;
    }

    screenInfo->offset = xBins[1].coordinate - xBins[0].coordinate;
    screenInfo->y = yBins[0].coordinate - screenInfo->offset / 2;
    screenInfo->x = xBins[0].coordinate - screenInfo->offset / 2;

    free(points);
    free(xBins);
    free(yBins);

    return size;
}


uint32_t getPoints(image_t img, coord_t **points, int pixelOffset) {

    coord_t *coords = malloc(32 * sizeof(coord_t));
    uint32_t coords_n = 32;
    uint32_t coords_i = 0;

    const uint32_t margin = WINDOW_BORDER_MARGIN;

    for (uint32_t y = margin; y < img.height - margin; y++) {
        for (uint32_t x = margin; x < img.width - margin; x++) {

            pixel_t *pix = getPix(img, x, y);

            // Did we find a discrepancy?
            if (isCrossing(img, pix, pixelOffset)) {
                // Expand the vector if it's too small.
                if (coords_i == coords_n) {
                    coords_n += 32;
                    coords = realloc(coords, coords_n * sizeof(coord_t));
                }
                coords[coords_i] = (coord_t){x, y};
                coords_i++;
            }
            // We literally use the green pixels, so we can't have actual
            // green pixels in the image.
            else if (pix->g == 255) pix->g = 254;
        }
    }

    if (coords_i == 0) {
        free(coords);
        *points = NULL;
        return 0;
    }

    coords = realloc(coords, coords_i * sizeof(coord_t));
    *points = coords;

    return coords_i;
}


// Finds the crossing and also fucks up the green channel of the image.
uint8_t isCrossing(image_t img, pixel_t *pix, int pixelOffset) {
    // Check if the pixels above, to the left and to the top left of this pixel
    // aren't already green.
    if (
           (pix - 1)->g == 255
        || (pix - img.width)->g == 255
        || (pix - 1 - img.width)->g == 255
    ) {
        if (pix->g == 255) pix->g = 254;
        return 0;
    }


    // 1 is black, 0 is not black.
    uint8_t conv[3][3] = {
        {0, 1, 0},
        {1, 1, 1},
        {0, 1, 0}
    };

    // Loop until we find a discrepancy
    for (int cy = -1; cy <= 1; cy++) {
        for (int cx = -1; cx <= 1; cx++) {

            int32_t offset = pixelOffset * cx + (pixelOffset * cy * img.width);
            pixel_t *pixPtr = pix + offset;

            pixel_t testPix = *pixPtr;

            if (isBlack(testPix) != conv[cy + 1][cx + 1]) {
                return 0;
            }

        }
    }

    pix->g = 255;
    pix->r /= 2;
    pix->b /= 2;
    return 1;
}


coord_t getBins(
    coord_t *points, uint32_t pointCount, bin_t *xBins, bin_t *yBins
) {
    uint32_t xBins_i = 0;
    uint32_t yBins_i = 0;

    for (uint32_t p = 0; p < pointCount; p++) {

        uint8_t newXBin = 1;
        for (uint32_t bin = 0; bin < xBins_i; bin++) {
            int32_t dif = abs(xBins[bin].coordinate - (int32_t)points[p].x);
            if (dif < BIN_MARGIN) {
                xBins[bin].count++;
                newXBin = 0;
                break;
            }
        }
        if (newXBin) {
            xBins[xBins_i].coordinate = points[p].x;
            xBins[xBins_i].count = 1;
            xBins_i++;
        }

        uint8_t newYBin = 1;
        for (uint32_t bin = 0; bin < yBins_i; bin++) {
            int32_t dif = abs(yBins[bin].coordinate - (int32_t)points[p].y);
            if (dif < BIN_MARGIN) {
                yBins[bin].count++;
                newYBin = 0;
                break;
            }
        }
        if (newYBin) {
            yBins[yBins_i].coordinate = points[p].y;
            yBins[yBins_i].count = 1;
            yBins_i++;
        }
    }

    sanitizeBins(xBins, &xBins_i, yBins, &yBins_i);

    return (coord_t){xBins_i, yBins_i};
}


void sanitizeBins(
    bin_t *xBins, uint32_t *xBins_i, bin_t *yBins, uint32_t *yBins_i
) {

    DPRINTF("xBins: ");
    for (uint32_t i = 0; i < *xBins_i; i++) {
        DPRINTF("[%d: %d] ", xBins[i].coordinate, xBins[i].count);

        // Invalid bin. Probably some other crossing.
        if (xBins[i].count < 3) {
            xBins[i] = xBins[*xBins_i - 1];
            *xBins_i -= 1;
            i--;
            continue;
        }
    }
    DPRINTF("\nyBins: ");
    for (uint32_t i = 0; i < *yBins_i; i++) {
        DPRINTF("[%d: %d] ", yBins[i].coordinate, yBins[i].count);

        // Invalid bin. Probably some other crossing.
        if (yBins[i].count < 3) {
            yBins[i] = yBins[*yBins_i - 1];
            *yBins_i -= 1;
            i--;
            continue;
        }
    }

    // Weird situation alert
    while (*xBins_i != *yBins_i) {
        bin_t *badBins;
        uint32_t *badBins_i;

        if (*xBins_i > *yBins_i) {
            badBins = xBins;
            badBins_i = &*xBins_i;
        }
        else {
            badBins = yBins;
            badBins_i = &*yBins_i;
        }

        uint32_t minBin = badBins[0].count;
        uint32_t minBin_i = 0;

        for (uint32_t i = 1; i < MAX(*xBins_i, *yBins_i); i++) {
            if (badBins[i].count < minBin) {
                minBin = badBins[i].count;
                minBin_i = i;
            }
        }

        badBins[minBin_i] = badBins[*badBins_i];
        *badBins_i -= 1;
    }


    printf("\nSorting them...\n");
    qsort(xBins, *xBins_i, sizeof(bin_t), compare_bins);
    qsort(yBins, *yBins_i, sizeof(bin_t), compare_bins);
}


static void fillBlob(
    uint32_t *board, uint32_t size, coord_t pos, uint32_t setVal
);

uint32_t *findColors(image_t img, bin_t *xBins, bin_t *yBins, uint32_t size) {

    uint32_t cellDistance = xBins[1].coordinate - xBins[0].coordinate;
    uint32_t colors_i = 0;
    pixel_t colors[size];

    coord_t origin = (coord_t){
        xBins[0].coordinate - cellDistance / 2,
        yBins[0].coordinate - cellDistance / 2
    };

    uint32_t *board = malloc(size * size * sizeof(uint32_t));

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            pixel_t *pixel = getPix(img,
                origin.x + cellDistance * x,
                origin.y + cellDistance * y
            );

            // Check if color already found.
            uint8_t colorFound = 0;
            for (uint32_t c = 0; c < colors_i; c++) {
                if (memcmp(colors + c, pixel, sizeof(pixel_t)) == 0) {
                    colorFound = 1;
                    board[x + y * size] = c;
                    break;
                }
            }
            if (colorFound) {
                continue;
            }


            if (colors_i >= size) {
                printf("FUCK there are more colors than queens.\n");
                free(board);
                return NULL;
            }
            board[x + y * size] = colors_i;
            colors[colors_i++] = *pixel;
            pixel->r = 255;
        }
    }

    if (colors_i == size) {
        return board;
    }

    if (colors_i > size) {
        fprintf(stderr,
            "There are too many colors??? I ain't dealing with that."
        );
        free(board);
        return NULL;
    }

    // Ok so if we get here, there's groups with the same color.
    // Not necessarily a problem, but we need to account for it.

    DPRINTF(
        "There are not enough colors. Assuming attached groups.\n"
        "Running attached group algorithm..\n"
    );

    for (uint32_t i = 0; i < size * size; i++) {
        printf("%d ", board[i]);
        if (i % size == size - 1) printf("\n");
    }


    colors_i = 0;

    // Iterate over the board and replace all the colors with new ones,
    // with bit 31 set to 1.
    for (uint32_t i = 0; i < size * size; i++) {
        if (board[i] & (1 << 31)) continue;

        coord_t pos = {i % size, i / size};
        fillBlob(board, size, pos, colors_i | (1 << 31));
        colors_i++; // Keep track of the amount of blobs we place.
    }

    // Unflip bit 31 for good measure (and debugability) (and preventing segfaults for some).
    for (uint32_t i = 0; i < size * size; i++) {
        board[i] ^= (1 << 31);
    }

    if (colors_i != size) {
        printf(
            "There are too few colors, "
            "even after identical group color resolution!!!\n"
        );
        free(board);
        return NULL;
    }

    #if DEBUG_PRINT_MODE
    for (uint32_t i = 0; i < colors_i; i++) {
        printf("Color %d: #%02x%02x%02x\n", i,
            colors[i].r, colors[i].g, colors[i].b
        );
    }
    #endif

    return board;
}

// Recursively replace all values equal to
// the value on the starting pos to setVal
void fillBlob(
    uint32_t *board, uint32_t size, coord_t pos, uint32_t setVal
) {
    const uint32_t index = pos.x + pos.y * size;
    const uint32_t replaceVal = board[index];
    board[index] = setVal;

    for (uint8_t i = 0; i < 4; i++) {
        coord_t newPos;
        // Check if it isn't out of bounds.
        switch (i) {
        case 0:
            newPos = (coord_t) {pos.x + 1, pos.y};
            if (newPos.x >= size) continue;
            break;
        case 1:
            newPos = (coord_t) {pos.x, pos.y + 1};
            if (newPos.y >= size) continue;
            break;
        case 2:
            newPos = (coord_t) {pos.x - 1, pos.y};
            if (newPos.x < 0) continue;
            break;
        case 3:
            newPos = (coord_t) {pos.x, pos.y - 1};
            if (newPos.y < 0) continue;
            break;
        }

        // Check if it is the value we are replacing.
        if (board[newPos.x + newPos.y * size] != replaceVal) continue;

        fillBlob(board, size, newPos, setVal);
    }
}


static inline uint8_t isBlack(pixel_t pixel) {
    return pixel.r < BLACK_THRESHOLD
        && pixel.g < BLACK_THRESHOLD
        && pixel.b < BLACK_THRESHOLD;
}


static inline uint16_t sum(pixel_t pixel) {
    return pixel.r + pixel.g + pixel.b;
}


static inline pixel_t* getPix(image_t img, uint32_t x, uint32_t y) {
    return img.pixels + (x + y * img.width);
}


#undef DEBUG_PRINT_MODE
#define DEBUG_PRINT_MODE PRINT_NORMAL