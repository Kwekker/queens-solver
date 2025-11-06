#ifndef LOOKER_H_
#define LOOKER_H_

#include <X11/X.h>
#include <X11/Xlib.h>

#include <stdint.h>


typedef struct {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        uint8_t rgb[3];
    };
} pixel_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    pixel_t *pixels;
} image_t;


typedef struct {
    int32_t x;
    int32_t y;
} coord_t;



// Looking
Window findBrowser(Display *display, Window root, int depth);

// Gets an image of the browser window.
// If browser == 0 it will try to find it by itself (using findBrowser).
image_t getBrowserWindow(Window browser, coord_t *browser_coords);
void imageToFile(
    const char *fileName, pixel_t *image, uint32_t width, uint32_t height
);

// Acting
// Initializes clicking (gets a display pointer and stores it in a static).
void initClicking(void);
// Should always be run after all necessary clicking has been completed.
void stopClicking(void);
void moveMouseTo(int x, int y);
void clickMouse(void);
void doubleClickMouse(void);

Window waitForActivation(Window browser);
    

#endif




// int main(int argc, char *argv[]) {
//     Display *display = XOpenDisplay(NULL);
//     Window root = DefaultRootWindow(display);

//     Window viv = findBrowser(display, root, 0);

//     // printProperties(display, viv);

//     screenshotToFile(display, "screenshot.ppm", viv);


//     return 0;
// }

