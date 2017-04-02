#include <sys/time.h>

#include "allegro5/allegro.h"
#include "allegro5/platform/aintpandora.h"
#include "allegro5/internal/aintern_pandora.h"
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

static ALLEGRO_SYSTEM_INTERFACE *pando_vt;


static ALLEGRO_SYSTEM *pando_initialize(int flags)
{
   (void)flags;

   ALLEGRO_SYSTEM_PANDORA *s;

//   bcm_host_init();

   s = (ALLEGRO_SYSTEM_PANDORA*)al_calloc(1, sizeof *s);

   _al_vector_init(&s->system.displays, sizeof (ALLEGRO_DISPLAY_PANDORA *));

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

   s->system.vt = pando_vt;

   XInitThreads();

   return &s->system;
}

static void pando_shutdown_system(void)
{

   ALLEGRO_SYSTEM *s = al_get_system_driver();
   ALLEGRO_SYSTEM_PANDORA *spnd = (ALLEGRO_SYSTEM_PANDORA*)s;

   ALLEGRO_INFO("shutting down.\n");

   /* Close all open displays. */
   while (_al_vector_size(&s->displays) > 0) {
      ALLEGRO_DISPLAY **dptr = (ALLEGRO_DISPLAY**)_al_vector_ref(&s->displays, 0);
      ALLEGRO_DISPLAY *d = *dptr;
      al_destroy_display(d);
   }
   _al_vector_free(&s->displays);

   if (getenv("DISPLAY")) {
      _al_thread_join(&spnd->thread);
      /*XSync(spnd->x11display, True);
      XCloseDisplay(spnd->x11display);*/
   }

   //raise(SIGINT);

   al_free(spnd);

}

static ALLEGRO_KEYBOARD_DRIVER *pando_get_keyboard_driver(void)
{
   if (getenv("DISPLAY")) {
      return _al_xwin_keyboard_driver();
   }
   return (ALLEGRO_KEYBOARD_DRIVER*)_al_linux_keyboard_driver_list[0].driver;
}

static ALLEGRO_MOUSE_DRIVER *pando_get_mouse_driver(void)
{
   if (getenv("DISPLAY")) {
      return _al_xwin_mouse_driver();
   }
   return (ALLEGRO_MOUSE_DRIVER*)_al_linux_mouse_driver_list[0].driver;
}

static ALLEGRO_JOYSTICK_DRIVER *pando_get_joystick_driver(void)
{
   return (ALLEGRO_JOYSTICK_DRIVER*)_al_joystick_driver_list[0].driver;
}

static int pando_get_num_video_adapters(void)
{
   return 1;
}

static bool pando_get_monitor_info(int adapter, ALLEGRO_MONITOR_INFO *info)
{
   (void)adapter;
   int dx, dy, w, h;
   _al_pandora_get_screen_info(&dx, &dy, &w, &h);
   info->x1 = 0;
   info->y1 = 0;
   info->x2 = w;
   info->y2 = h;
   return true;
}

static bool pando_get_cursor_position(int *ret_x, int *ret_y)
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

static bool pando_inhibit_screensaver(bool inhibit)
{
   ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA*)al_get_system_driver();

   system->inhibit_screensaver = inhibit;
   return true;
}

static int pando_get_num_display_modes(void)
{
   return 1;
}

static ALLEGRO_DISPLAY_MODE *pando_get_display_mode(int mode, ALLEGRO_DISPLAY_MODE *dm)
{
   (void)mode;
   int dx, dy, w, h;
   _al_pandora_get_screen_info(&dx, &dy, &w, &h);
   dm->width = w;
   dm->height = h;
   dm->format = 0; // FIXME
   dm->refresh_rate = 60;
   return dm;
}

static ALLEGRO_MOUSE_CURSOR *pando_create_mouse_cursor(ALLEGRO_BITMAP *bmp, int focus_x_ignored, int focus_y_ignored)
{
   (void)focus_x_ignored;
   (void)focus_y_ignored;

   ALLEGRO_STATE state;
   al_store_state(&state, ALLEGRO_STATE_NEW_BITMAP_PARAMETERS | ALLEGRO_STATE_TARGET_BITMAP);
   al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ARGB_8888);
   al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
   ALLEGRO_BITMAP *cursor_bmp = al_clone_bitmap(bmp);
   ALLEGRO_MOUSE_CURSOR_PANDORA *cursor = al_malloc(sizeof(ALLEGRO_MOUSE_CURSOR_PANDORA));
   cursor->bitmap = cursor_bmp;
   al_restore_state(&state);
   return (ALLEGRO_MOUSE_CURSOR *)cursor;
}

static void pando_destroy_mouse_cursor(ALLEGRO_MOUSE_CURSOR *cursor)
{
   ALLEGRO_MOUSE_CURSOR_PANDORA *pnd_cursor = (void *)cursor;
   al_destroy_bitmap(pnd_cursor->bitmap);
   al_free(pnd_cursor);
}

//ALLEGRO_MOUSE_DRIVER _al_mousedrv_linux_evdev;

/* Internal function to get a reference to this driver. */
ALLEGRO_SYSTEM_INTERFACE *_al_system_pandora_driver(void)
{
   if (pando_vt)
      return pando_vt;

   pando_vt = (ALLEGRO_SYSTEM_INTERFACE*)al_calloc(1, sizeof *pando_vt);

   pando_vt->initialize = pando_initialize;
   pando_vt->get_display_driver = _al_get_pandora_display_interface;
   pando_vt->get_keyboard_driver = pando_get_keyboard_driver;
   pando_vt->get_mouse_driver = pando_get_mouse_driver;
   pando_vt->get_joystick_driver = pando_get_joystick_driver;
   pando_vt->get_num_display_modes = pando_get_num_display_modes;
   pando_vt->get_display_mode = pando_get_display_mode;
   pando_vt->shutdown_system = pando_shutdown_system;
   pando_vt->get_num_video_adapters = pando_get_num_video_adapters;
   pando_vt->get_monitor_info = pando_get_monitor_info;
   pando_vt->create_mouse_cursor = pando_create_mouse_cursor;//_al_xwin_create_mouse_cursor; // FIXME
   pando_vt->destroy_mouse_cursor = pando_destroy_mouse_cursor;//_al_xwin_destroy_mouse_cursor; // FIXME
   pando_vt->get_cursor_position = pando_get_cursor_position; // FIXME
   pando_vt->grab_mouse = _al_xwin_grab_mouse;
   pando_vt->ungrab_mouse = _al_xwin_ungrab_mouse;
   pando_vt->get_path = _al_unix_get_path;
   pando_vt->inhibit_screensaver = pando_inhibit_screensaver;

   return pando_vt;
}


/* vim: set sts=3 sw=3 et: */
