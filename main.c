#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "clicker.h"
#include "looker.h"
#include "reader.h"
#include "seeer.h"
#include "solver.h"
#include "types.h"


#define S(s) S_LAYER(s)
#define S_LAYER(s) #s

void printHelp(char *executable);
void printFileHelp(void);


int main(int argc, char *argv[]) {

    struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"window", required_argument, 0, 'w'},
        {"delay", required_argument, 0, 'd'},
        {"crossing-offset", required_argument, 0, 'c'},
        {"no-click", no_argument, 0, 'n'},
        {"no-wait", no_argument, 0, 'W'},
        {"no-solve", no_argument, 0, 's'},
        {"export-image", no_argument, 0, 'e'},
        {"max-attempts", required_argument, 0, 'm'},
        {"help", no_argument, 0, 'h'},
        {"help-file", no_argument, 0, '*'},
        {0, 0, 0, 0}
    };

    uint32_t click_delay = 800;
    Window input_window = 0;
    uint8_t export_image = 0;
    uint8_t dont_click_solve = 0;
    uint8_t dont_wait = 0;
    uint8_t dont_solve = 0;
    int32_t max_attempts = 0;
    uint32_t crossing_offset = 5;
    FILE *file = NULL;

    while (1) {
        int option_index = 0;

        int opt = getopt_long(argc, argv, "f:w:d:c:nWsmhe", long_options, &option_index);
        if (opt == -1) {
            break;
        }

        switch (opt) {

            case 'n':
                dont_click_solve = 1;
                break;

            case 'W':
                dont_wait = 1;
                break;

            case 'm':
                max_attempts = strtol(optarg, NULL, 0);
                break;

            case 's':
                dont_solve = 1;
                break;

            case 'f':
                file = fopen(optarg, "r");
                if (file == NULL) {
                    fprintf(stderr, "File %s not found.\n", optarg);
                    return -1;
                }
                break;

            case 'd':
                click_delay = strtol(optarg, NULL, 0);
                if (click_delay == 0) {
                    fprintf(stderr,
                        "Could not parse delay number.\n"
                        "A delay of 0 is not supported (use 1)\n"
                    );
                    return -1;
                }
                break;

            case 'c':
                crossing_offset = strtol(optarg, NULL, 0);
                if (click_delay == 0) {
                    fprintf(stderr,
                        "Could not parse crossing offset.\n"
                        "A crossing offset of 0 doesn't work.\n"
                    );
                    return -1;
                }
                break;

            case 'w':
                printf("Yoo window\n");
                input_window = strtol(optarg, NULL, 0);
                if (input_window == 0) {
                    fprintf(stderr, "Could not parse window number.\n");
                }
                break;

            case 'e':
                export_image = 1;
                break;

            case 'h':
                printHelp(argv[0]);
                return 0;

            case '*':
                printFileHelp();
                return 0;


            default:
                fprintf(stderr, "getopt returned character code 0x%x??\n", opt);
        }
    }

    board_t board;
    boardScreenInfo_t screenInfo = {0};

    // Automatic board detection
    if (file == NULL) {

        if (!dont_wait) {
            input_window = waitForActivation(input_window);
        }

        uint32_t size = 0;

        image_t image;
        uint32_t *colors;
        for (uint32_t i = 0; i < max_attempts || max_attempts == 0; i++) {
            //* Finding the browser window.
            if (input_window) printf("Using your window: %lx\n", input_window);

            coord_t browser_coords;
            image = getBrowserWindow(input_window, &browser_coords);

            size = detectBoard(image, &colors, &screenInfo, crossing_offset);
            screenInfo.x += browser_coords.x;
            screenInfo.y += browser_coords.y;

            if (export_image) {
                imageToFile(
                    "img/export.ppm", image.pixels, image.width, image.height
                );
            }

            if (size) break;


            free(image.pixels);
        }


        if (size == 0) {
            fprintf(stderr,
                "Could not detect a valid board within %d attempts.",
                max_attempts
            );
            return -1;
        }



        // Create and color the board.
        board = createBoard(size);
        colorBoard(board, colors);
        if (dont_solve) {
            printf("Detected this board:\n");
            printBoard(board, 0);
            free(image.pixels);
            free(colors);
            freeBoard(board);
            return 0;
        }
        printf("Solving this board:\n");
        printBoard(board, 0);
        printf("\n");

        free(image.pixels);
        free(colors);

        // Solve the board.
        board = solve(board);

        printf("\nSolved!\n");
        if (!dont_click_solve) {
            printf("Clicking..\n");
            clickSolveBoard(board, screenInfo, click_delay);
        }

        printf("The board:\n");
        printBoard(board, 0);

        printf("\n");

        freeBoard(board);
    }
    // File reading
    else {

        uint32_t size = 0;

        if (file == NULL) {
            fprintf(stderr, "Your stupid file doesn't exist nerd.\n");
            return -1;
        }

        //* Reading file
        if (measureQueensFile(file, &size)) {
            fprintf(stderr, "Something went wrong while measuring the file.\n");
            return -1;
        }
        printf("Size: %d\n", size);

        board = createBoard(size);

        fseek(file, 0, SEEK_SET);
        if (readQueensFile(file, &board)) {
            fprintf(stderr, "Reading the board went wrong somehow whoops\n");
            freeBoard(board);
            return -1;
        }
    }
}


void printHelp(char *executable) {
    printf("Usage: %s [OPTION]\n", executable);
    printf(
        "This program can solve a Queens (or Star Battle) puzzle.\n"
        "It can also fill in the answer for you at very high speeds,\n"
        "by taking control of your mouse.\n\n"

        "Default behaviour is:\n"
        "1. Find browser window\n"
        "2. Wait for user to activate browser window\n"
        "3. Repeatedly take a screenshot of the browser window\n"
        "   until a Queens board is detected\n"
        "4. Read and solve the Queens board\n"
        "5. Fill in the answer by double clicking on the queen positions with the mouse\n\n"

        "  -f, --file=FILE      Read the board from FILE instead of the screen.\n"
        "                       For the board file format use --help-file.\n"
        "  -d, --delay=DELAY    The delay between clicks in us.\n"
        "                       Some websites need longer delays.\n"
        "  -n, --no-click       Don't take control of the mouse, just print the solution.\n"
        "                       Has no effect with -f\n"
        "  -w, --window=WINDOW  Use the window with id WINDOW instead of\n"
        "                       automatically detecting it. Window id can be obtained\n"
        "                       using xwininfo. Has no effect with -f\n"
        "  -m, --max-attempts=ATTEMPTS\n"
        "                       Attempt to detect a board a maximum of ATTEMPTS times.\n"
        "                       Default is 0 (no limit).\n"
        "  -W, --no-wait        Don't wait for window activation.\n"
        "  -h, --help           Display this help and exit\n\n"
        "      --help-file      Display a help text about the file format for the -f option.\n\n"

    );
}


void printFileHelp(void) {
    printf(
        "File format for a board file:\n\n"

        "n * {TOKEN}{WHITESPACE}\n"
        "    n is a square number,\n"
        "    TOKEN is any amount of non-whitespace characters,\n"
        "    and WHITESPACE is any amount of whitespace characters (including newlines).\n\n"

        "Tokens are used to indicate in which group a cell belongs (which color it is).\n"
        "Identical tokens are given the same color.\n\n\n"

        "Example:\n"
        "  a a a a a\n"
        "  a a b a c\n"
        "  d b b c c\n"
        "  d e e c c\n"
        "  d e e c c\n\n"
        "Results in the following Queens board:\n"
        " _________  \n"
        "|    _   _| \n"
        "|___| |_| | \n"
        "| |___|   | \n"
        "| |   |   | \n"
        "|_|___| __| \n\n"

        "But the following file also does:\n"
        "  red red red red red red red 2 red & blue 2 2 & &\n"
        "  blue e e & & blue e e & &\n"
    );
}