#include "looker.h"
#include "debug_prints.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

// gcc -o seeer seeer.c -Wall -lX11

#define PRINT_PROPERTIES_IN_TREE


static uint8_t isBrowser(Display *display, Window window);
[[maybe_unused]]
static void screenshotToFile(Display *display, char *fileName, Window window);
[[maybe_unused]]
static void printKids(Display *display, Window *kids, uint32_t kidCount, uint32_t depth);
[[maybe_unused]]
static void printProperties(Display *display, Window window);
[[maybe_unused]]
static void printKids(Display *display, Window *kids, uint32_t kidCount, uint32_t depth);


image_t getBrowserWindow(Window browser, coord_t *browser_coords) {

    image_t ret = {0};

    Display *display = XOpenDisplay(NULL);
    Window root = DefaultRootWindow(display);

    if (browser == 0) {
        browser = findBrowser(display, root, 0);
    }

    // Get window attributes (width, height).
    XWindowAttributes browser_attrs;
    XGetWindowAttributes(display, browser, &browser_attrs);
    int width = browser_attrs.width;
    int height = browser_attrs.height;

    // Get the window's image.
    DPRINTF("getting image of [%d, %d]\n", width, height);
    XImage *image =
        XGetImage(display, browser, 0, 0, width, height, AllPlanes, ZPixmap);


    if (image == NULL) {
        DPRINTF("Window is cringe\n");
        XFree(display);
        return ret;
    }
    DPRINTF("Got image\n");


    // Find browser coordinates.
    browser_coords->x = 0;
    browser_coords->y = 0;
    Window child = browser;
    while (1) {
        XWindowAttributes child_attrs;
        XGetWindowAttributes(display, child, &child_attrs);
        browser_coords->x += child_attrs.x;
        browser_coords->y += child_attrs.y;

        printf("Window 0x%lx adds %d, %d\n", child, child_attrs.x, child_attrs.y);

        if (child == root) break;

        Window parent;
        Window idc0;
        Window *kids;
        uint32_t idc1;
        XQueryTree(display, child, &idc0, &parent, &kids, &idc1);
        XFree(kids);

        child = parent;
    }

    printf("Browser coords: %d, %d\n", browser_coords->x, browser_coords->y);


    // Allocate the array.
    pixel_t *array = malloc(width * height * sizeof(pixel_t));

    unsigned long red_mask = image->red_mask;
    unsigned long green_mask = image->green_mask;
    unsigned long blue_mask = image->blue_mask;

    // Populate the array.
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned long pixel = image->f.get_pixel(image, x, y);

            unsigned char blue = pixel & blue_mask;
            unsigned char green = (pixel & green_mask) >> 8;
            unsigned char red = (pixel & red_mask) >> 16;

            array[(x + width * y)].r = red;
            array[(x + width * y)].g = green;
            array[(x + width * y)].b = blue;
        }
    }

    image->f.destroy_image(image);
    XCloseDisplay(display);


    ret.width = width;
    ret.height = height;
    ret.pixels = array;

    return ret;
}


Window waitForActivation(Window browser) {
    Display *display = XOpenDisplay(NULL);

    if (browser == 0) {
        Window root = DefaultRootWindow(display);
        browser = findBrowser(display, root, 0);
    }

    DPRINTF("Found window. Waiting for activation..\n");

    XRaiseWindow(display, browser);

    XSelectInput(display, browser, FocusChangeMask);
    XEvent event;
    while (1) {
        XNextEvent(display, &event);

        if (event.type == FocusIn) {
            DPRINTF("Window 0x%lx gained focus!\n", browser);
            XCloseDisplay(display);
            return browser;
        }
    }

}


void imageToFile(const char *fileName, pixel_t *image, uint32_t width, uint32_t height) {

    FILE *file = fopen(fileName, "w");
    fprintf(file, "P6\n%d\n%d\n255\n", width, height);


    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fwrite(image[y * width + x].rgb, 1, 3, file);
        }
    }

    fclose(file);
}


Window findBrowser(Display *display, Window root, int depth) {
    Window idc1, idc2;
    Window *kids;
    uint32_t kidCount = 0;
    XQueryTree(display, root, &idc1, &idc2, &kids, &kidCount);


    for (uint32_t i = 0; i < kidCount; i++) {

        if (isBrowser(display, kids[i])) {
            Window ret = kids[i];
            XFree(kids);
            return ret;
        }


        Window potentialBrowser = findBrowser(display, kids[i], depth + 1);
        if (potentialBrowser != 0) {
            XFree(kids);
            return potentialBrowser;
        }
    }

    XFree(kids);
    return 0;
}


uint8_t isBrowser(Display *display, Window window) {

    Atom atom = XInternAtom(display, "WM_WINDOW_ROLE", 1);
    if (atom == None) return 0;

    Atom type;
    int format;
    unsigned long nitems;
    unsigned long remaining;
    unsigned char *data;

    XGetWindowProperty(
        display,
        window,
        atom,
        0, 32, 0,
        AnyPropertyType,
        &type,
        &format,
        &nitems,
        &remaining,
        &data
    );

    if (nitems != 7) {
        XFree(data);
        return 0;
    }

    uint8_t ret = memcmp(data, "browser", 7) == 0;
    XFree(data);
    return ret;
}



void screenshotToFile(Display *display, char *fileName, Window window) {
    XWindowAttributes gwa;
    XGetWindowAttributes(display, window, &gwa);
    int width = gwa.width;
    int height = gwa.height;

    if (gwa.class == InputOnly) {
        printf("this fucker is InputOnly!\n");
        return;
    }

    printf("getting image of [%d, %d]\n", width, height);
    XImage *image =
        XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);

    if (image == NULL) {
        printf("Window is cringe\n");
        return;
    }
    printf("got image\n");


    unsigned char *array = malloc(width * height * 3);

    unsigned long red_mask = image->red_mask;
    unsigned long green_mask = image->green_mask;
    unsigned long blue_mask = image->blue_mask;

    FILE *file = fopen(fileName, "w");
    fprintf(file, "P6\n%d\n%d\n255\n", width, height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned long pixel = image->f.get_pixel(image, x, y);

            unsigned char blue = pixel & blue_mask;
            unsigned char green = (pixel & green_mask) >> 8;
            unsigned char red = (pixel & red_mask) >> 16;

            unsigned char rgb[3] = {red, green, blue};

            array[(x + width * y) * 3] = red;
            array[(x + width * y) * 3 + 1] = green;
            array[(x + width * y) * 3 + 2] = blue;

            fwrite(rgb, 1, 3, file);
        }
    }

    fclose(file);

    free(array);
}


void printProperties(Display *display, Window window) {
    int propertiesCount;
    Atom *properties = XListProperties(display, window, &propertiesCount);
    fprintf(stderr, "properties:\n");
    for (int p = 0; p < propertiesCount; p++) {
        char *atomName = XGetAtomName(display, properties[p]);

        Atom type;
        int format;
        unsigned long nitems;
        unsigned long remaining;
        unsigned char *data;

        XGetWindowProperty(
            display,
            window,
            properties[p],
            0, 32, 0,
            AnyPropertyType,
            &type,
            &format,
            &nitems,
            &remaining,
            &data
        );

        fprintf(stderr, "[%s]\n\t", atomName);
        for (int i = 0; i < 32; i++) {
            fprintf(stderr, "%2x ", data[i]);
        }
        fprintf(stderr, "\n\t");
        for (int i = 0; i < 32; i++) {
            if(data[i] > ' ' && data[i] < 0x7f)
                fprintf(stderr, " %c ", data[i]);
            else if (data[i] == ' ')
                fprintf(stderr, " \x1b[31m_\x1b[0m ");
            else
                fprintf(stderr, "   ");
        }
        fprintf(stderr, "\n%lu, %d, %lu, %lu\n", type, format, nitems, remaining);

    }
}


void printKids(Display *display, Window *kids, uint32_t kidCount, uint32_t depth) {

    for (uint32_t i = 0; i < kidCount; i++) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, kids[i], &attrs);

        for (uint32_t d = 0; d < depth; d++) fprintf(stderr, "\t");
        fprintf(stderr, "Kid \x1b[33m%d\x1b[0m: ", i);


        // Name
        char *name;
        XFetchName(display, kids[i], &name);

        fprintf(stderr, "\"\x1b[34m%s\x1b[0m\" (%lx) ", name, kids[i]);
        if (attrs.class == InputOutput) fprintf(stderr, "[\x1b[32mINOUT\x1b[0m] ");
        free(name);

#ifdef PRINT_PROPERTIES_IN_TREE
        int propertiesCount;
        Atom *properties = XListProperties(display, kids[i], &propertiesCount);
        fprintf(stderr, "properties:\n");
        for (int p = 0; p < propertiesCount; p++) {
            for (uint32_t d = 0; d < depth + 1; d++) fprintf(stderr, "\t");
            char *atomName = XGetAtomName(display, properties[p]);
            fprintf(stderr, "[%s]\n", atomName);
        }
#endif

        Window idc1, idc2;
        Window *kidsKids;
        uint32_t kidsKidCount = 0;
        XQueryTree(display, kids[i], &idc1, &idc2, &kidsKids, &kidsKidCount);

        if (kidsKidCount > 0) {
#ifdef PRINT_PROPERTIES_IN_TREE
            for (uint32_t d = 0; d < depth; d++) fprintf(stderr, "\t");
#endif
            printf("has %d kids:\n", kidsKidCount);
            printKids(display, kidsKids, kidsKidCount, depth + 1);
        }
#ifndef PRINT_PROPERTIES_IN_TREE
        else printf("\n");
#endif

        XFree(kidsKids);

    }
}


// ============ Acting ============

static Display *actingDisplay;

void initClicking(void) {
    actingDisplay = XOpenDisplay(NULL);
}

void stopClicking(void) {
    XCloseDisplay(actingDisplay);
}

static void getMouseCoords(int *x, int *y) {
    XEvent event;
    XQueryPointer (
        actingDisplay, DefaultRootWindow (actingDisplay),
        &event.xbutton.root, &event.xbutton.window,
        &event.xbutton.x_root, &event.xbutton.y_root,
        &event.xbutton.x, &event.xbutton.y,
        &event.xbutton.state
    );
    *x = event.xbutton.x;
    *y = event.xbutton.y;
}


void moveMouseTo(int x, int y) {
    int cur_x, cur_y;
    getMouseCoords(&cur_x, &cur_y);
    usleep (1);
    XWarpPointer(actingDisplay, None, None, 0,0,0,0, x - cur_x, y - cur_y);
    usleep (1);
}


void clickMouse(void) {
    XTestFakeButtonEvent(actingDisplay, Button1, True, CurrentTime);
    usleep (1);
    XTestFakeButtonEvent(actingDisplay, Button1, False, CurrentTime);
}


void doubleClickMouse(void) {
    XTestFakeButtonEvent(actingDisplay, Button1, True, CurrentTime);
    usleep (1);
    XTestFakeButtonEvent(actingDisplay, Button1, False, CurrentTime);
    usleep (10);
    XTestFakeButtonEvent(actingDisplay, Button1, True, CurrentTime);
    usleep (1);
    XTestFakeButtonEvent(actingDisplay, Button1, False, CurrentTime);
}