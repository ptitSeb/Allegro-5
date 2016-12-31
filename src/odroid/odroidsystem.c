#include <sys/time.h>

#include "allegro5/allegro.h"
#include "allegro5/platform/aintodroid.h"
#include "allegro5/internal/aintern_odroid.h"
#include "allegro5/platform/aintunix.h"
#include "allegro5/platform/aintlnx.h"
#include "allegro5/internal/aintern_x.h"
#include "allegro5/internal/aintern_xevents.h"
#include "allegro5/internal/aintern_xmouse.h"
#include "allegro5/internal/aintern_xcursor.h"
#include "allegro5/internal/aintern_xsystem.h"

#include "eglport.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>


#include <signal.h>

ALLEGRO_DEBUG_CHANNEL("system")

static ALLEGRO_SYSTEM_INTERFACE *odroid_vt;


static ALLEGRO_SYSTEM *odroid_initialize(int flags)
{
   (void)flags;

   ALLEGRO_SYSTEM_ODROID *s;

   s = (ALLEGRO_SYSTEM_ODROID*)al_calloc(1, sizeof *s);

   _al_vector_init(&s->system.displays, sizeof (ALLEGRO_DISPLAY_ODROID *));

   _al_unix_init_time();

   if (getenv("DISPLAY")) {
      _al_mutex_init_recursive(&s->lock);
      s->x11display = XOpenDisplay(0);
      _al_thread_create(&s->thread, _al_xwin_background_thread, s);
      ALLEGRO_INFO("events thread spawned.\n");
      /* We need to put *some* atom into the ClientMessage we send for
       * faking mouse movements with al_set_mouse_xy - so let's ask X11
       * for one here.
       */
      s->AllegroAtom = XInternAtom(s->x11display, "AllegroAtom", False);
   }

   s->inhibit_screensaver = false;

   s->system.vt = odroid_vt;

   return &s->system;
}

static void odroid_shutdown_system(void)
{

   ALLEGRO_SYSTEM *s = al_get_system_driver();
   ALLEGRO_SYSTEM_ODROID *spnd = (ALLEGRO_SYSTEM_ODROID*)s;

   ALLEGRO_INFO("shutting down.\n");

   /* Close all open displays. */
   while (_al_vector_size(&s->displays) > 0) {
      ALLEGRO_DISPLAY **dptr = (ALLEGRO_DISPLAY**)_al_vector_ref(&s->displays, 0);
      ALLEGRO_DISPLAY *d = *dptr;
      _al_destroy_display_bitmaps(d);
      al_destroy_display(d);
   }
   _al_vector_free(&s->displays);

   if (getenv("DISPLAY")) {
      _al_thread_join(&spnd->thread);
      XCloseDisplay(spnd->x11display);
   }

   raise(SIGINT);

   al_free(spnd);

}

static ALLEGRO_KEYBOARD_DRIVER *odroid_get_keyboard_driver(void)
{
   if (getenv("DISPLAY")) {
      return _al_xwin_keyboard_driver();
   }
   return (ALLEGRO_KEYBOARD_DRIVER*)_al_linux_keyboard_driver_list[0].driver;
}

static ALLEGRO_MOUSE_DRIVER *odroid_get_mouse_driver(void)
{
   if (getenv("DISPLAY")) {
      return _al_xwin_mouse_driver();
   }
   return (ALLEGRO_MOUSE_DRIVER*)_al_linux_mouse_driver_list[0].driver;
}

static ALLEGRO_JOYSTICK_DRIVER *odroid_get_joystick_driver(void)
{
   return (ALLEGRO_JOYSTICK_DRIVER*)_al_joystick_driver_list[0].driver;
}

static int odroid_get_num_video_adapters(void)
{
   return 1;
}

static bool odroid_get_monitor_info(int adapter, ALLEGRO_MONITOR_INFO *info)
{
   (void)adapter;
   int dx, dy, w, h;
   _al_odroid_get_screen_info(&dx, &dy, &w, &h);
   info->x1 = 0;
   info->y1 = 0;
   info->x2 = w;
   info->y2 = h;
   return true;
}

static bool odroid_get_cursor_position(int *ret_x, int *ret_y)
{
   ALLEGRO_SYSTEM_XGLX *system = (void *)al_get_system_driver();
   Window root = RootWindow(system->x11display, 0);
   Window child;
   int wx, wy;
   unsigned int mask;
	*ret_x=0; *ret_y=0;
   _al_mutex_lock(&system->lock);
   XQueryPointer(system->x11display, root, &root, &child, ret_x, ret_y,
      &wx, &wy, &mask);
   _al_mutex_unlock(&system->lock);
   return true;
}

static bool odroid_inhibit_screensaver(bool inhibit)
{
   ALLEGRO_SYSTEM_ODROID *system = (ALLEGRO_SYSTEM_ODROID*)al_get_system_driver();

   system->inhibit_screensaver = inhibit;
   return true;
}

static int odroid_get_num_display_modes(void)
{
   return 1;
}

static ALLEGRO_DISPLAY_MODE *odroid_get_display_mode(int mode, ALLEGRO_DISPLAY_MODE *dm)
{
   (void)mode;
   int dx, dy, w, h;
   _al_odroid_get_screen_info(&dx, &dy, &w, &h);
   dm->width = w;
   dm->height = h;
   dm->format = 0; // FIXME
   dm->refresh_rate = 60;
   return dm;
}

static ALLEGRO_MOUSE_CURSOR *odroid_create_mouse_cursor(ALLEGRO_BITMAP *bmp, int focus_x_ignored, int focus_y_ignored)
{
   (void)focus_x_ignored;
   (void)focus_y_ignored;

   ALLEGRO_STATE state;
   al_store_state(&state, ALLEGRO_STATE_NEW_BITMAP_PARAMETERS | ALLEGRO_STATE_TARGET_BITMAP);
   al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ARGB_8888);
   al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
   ALLEGRO_BITMAP *cursor_bmp = al_clone_bitmap(bmp);
   ALLEGRO_MOUSE_CURSOR_ODROID *cursor = al_malloc(sizeof(ALLEGRO_MOUSE_CURSOR_ODROID));
   cursor->bitmap = cursor_bmp;
   al_restore_state(&state);
   return (ALLEGRO_MOUSE_CURSOR *)cursor;
}

static void odroid_destroy_mouse_cursor(ALLEGRO_MOUSE_CURSOR *cursor)
{
   ALLEGRO_MOUSE_CURSOR_ODROID *pnd_cursor = (void *)cursor;
   al_destroy_bitmap(pnd_cursor->bitmap);
   al_free(pnd_cursor);
}

//ALLEGRO_MOUSE_DRIVER _al_mousedrv_linux_evdev;

/* Internal function to get a reference to this driver. */
ALLEGRO_SYSTEM_INTERFACE *_al_system_odroid_driver(void)
{
   if (odroid_vt)
      return odroid_vt;

   odroid_vt = (ALLEGRO_SYSTEM_INTERFACE*)al_calloc(1, sizeof *odroid_vt);

   odroid_vt->initialize = odroid_initialize;
   odroid_vt->get_display_driver = _al_get_odroid_display_interface;
   odroid_vt->get_keyboard_driver = odroid_get_keyboard_driver;
   odroid_vt->get_mouse_driver = odroid_get_mouse_driver;
   odroid_vt->get_joystick_driver = odroid_get_joystick_driver;
   odroid_vt->get_num_display_modes = odroid_get_num_display_modes;
   odroid_vt->get_display_mode = odroid_get_display_mode;
   odroid_vt->shutdown_system = odroid_shutdown_system;
   odroid_vt->get_num_video_adapters = odroid_get_num_video_adapters;
   odroid_vt->get_monitor_info = odroid_get_monitor_info;
   odroid_vt->create_mouse_cursor = odroid_create_mouse_cursor;//_al_xwin_create_mouse_cursor; // FIXME
   odroid_vt->destroy_mouse_cursor = odroid_destroy_mouse_cursor;//_al_xwin_destroy_mouse_cursor; // FIXME
   odroid_vt->get_cursor_position = odroid_get_cursor_position; // FIXME
   odroid_vt->grab_mouse = _al_xwin_grab_mouse;
   odroid_vt->ungrab_mouse = _al_xwin_ungrab_mouse;
   odroid_vt->get_path = _al_unix_get_path;
   odroid_vt->inhibit_screensaver = odroid_inhibit_screensaver;

   return odroid_vt;
}


/* vim: set sts=3 sw=3 et: */
