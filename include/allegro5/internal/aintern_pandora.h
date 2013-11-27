#ifndef _ALLEGRO_INTERNAL_PANDORA_SYSTEM
#define _ALLEGRO_INTERNAL_PANDORA_SYSTEM

#include "allegro5/allegro.h"
#include <allegro5/internal/aintern_system.h>
#include <allegro5/internal/aintern_display.h>
#include <allegro5/internal/aintern_mouse.h>

#include <X11/Xlib.h>
/*
#ifndef ALLEGRO_PANDORA
#define ALLEGRO_PANDORA
//#define ALLEGRO_RASPBERRYPI
#endif
*/
typedef struct ALLEGRO_SYSTEM_PANDORA {
   ALLEGRO_SYSTEM system;
   Display *x11display;
   _AL_MUTEX lock; /* thread lock for whenever we access internals. */
   _AL_THREAD thread; /* background thread. */
   ALLEGRO_DISPLAY *mouse_grab_display; /* Best effort: may be inaccurate. */
   bool inhibit_screensaver; /* Should we inhibit the screensaver? */
   Atom AllegroAtom;
} ALLEGRO_SYSTEM_PANDORA;

typedef struct ALLEGRO_DISPLAY_PANDORA_EXTRA
               ALLEGRO_DISPLAY_PANDORA_EXTRA;

typedef struct ALLEGRO_DISPLAY_PANDORA {
   ALLEGRO_DISPLAY display;
   ALLEGRO_DISPLAY_PANDORA_EXTRA *extra;
   Window window;
   /* Physical size of display in pixels (gets scaled) */
   int screen_width, screen_height;
   /* Cursor stuff */
//   ALLEGRO_MOUSE_CURSOR_XWIN *current_cursor;
   /* al_set_mouse_xy implementation */
   bool mouse_warp;
   bool cursor_hidden;
   uint32_t current_cursor;
   uint32_t invisible_cursor;
   uint8_t cursor_data[10000]; // FIXME: max 50x50 cursor @ 32 bit
   int cursor_offset_x, cursor_offset_y;
   Atom wm_delete_window_atom;
} ALLEGRO_DISPLAY_PANDORA;

typedef struct ALLEGRO_MOUSE_CURSOR_PANDORA {
   ALLEGRO_BITMAP *bitmap;
} ALLEGRO_MOUSE_CURSOR_PANDORA;

ALLEGRO_SYSTEM_INTERFACE *_al_system_pandora_driver(void);
ALLEGRO_DISPLAY_INTERFACE *_al_get_pandora_display_interface(void);
void _al_pandora_get_screen_info(int *dx, int *dy, int *screen_width, int *screen_height);

bool _al_evdev_set_mouse_range(int x1, int y1, int x2, int y2);

ALLEGRO_MOUSE_DRIVER _al_mousedrv_linux_evdev;

#endif
