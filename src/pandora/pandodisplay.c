#include <stdio.h>
#include <X11/extensions/Xfixes.h>

#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"
#include "allegro5/internal/aintern_opengl.h"
#include "allegro5/internal/aintern_vector.h"
#include "allegro5/internal/aintern_pandora.h"
#include "allegro5/internal/aintern_x.h"
#include "allegro5/internal/aintern_xwindow.h"

ALLEGRO_DEBUG_CHANNEL("display")

#define PANDORA
#define USE_EGL_RAW
#ifndef ALLEGRO_CFG_OPENGLES2
#define USE_GLES1
#else
#define USE_GLES2
#endif
#include "eglport.h"

// with USE_XCURSOR, the function from x/xcursor.c are used.
#define USE_XCURSOR

#ifdef USE_XCURSOR
#include "allegro5/internal/aintern_xcursor.h"
#endif

#define PITCH 128
#define DEFAULT_CURSOR_WIDTH 17
#define DEFAULT_CURSOR_HEIGHT 28

static float mouse_scale_ratio_x = 1.0f, mouse_scale_ratio_y = 1.0f;

static const ALLEGRO_XWIN_DISPLAY_OVERRIDABLE_INTERFACE default_overridable_vt;
static const ALLEGRO_XWIN_DISPLAY_OVERRIDABLE_INTERFACE *gtk_override_vt = NULL;

static ALLEGRO_DISPLAY_INTERFACE *vt;

struct ALLEGRO_DISPLAY_PANDORA_EXTRA {
};

/* Helper to set up GL state as we want it. */
static void setup_gl(ALLEGRO_DISPLAY *d)
{
    ALLEGRO_OGL_EXTRAS *ogl = d->ogl_extras;

    if (ogl->backbuffer)
        _al_ogl_resize_backbuffer(ogl->backbuffer, d->w, d->h);
    else
        ogl->backbuffer = _al_ogl_create_backbuffer(d);
}

void _al_pandora_get_screen_info(int *dx, int *dy, int *screen_width, int *screen_height)
{
//printf("_al_pandora_get_screen_info(int *dx, int *dy, int *screen_width, int *screen_height)\n");
//   graphics_get_display_size(0 /* LCD */, (uint32_t *)screen_width, (uint32_t *)screen_height);
   (*screen_width)=800;
   (*screen_height)=480;
   (*dx)=0;
   (*dy)=0;
}

static ALLEGRO_DISPLAY *pandora_create_display(int w, int h)
{
    ALLEGRO_DISPLAY_PANDORA *d = (ALLEGRO_DISPLAY_PANDORA*)al_calloc(1, sizeof *d);
    ALLEGRO_DISPLAY *display = (ALLEGRO_DISPLAY*)d;
    ALLEGRO_OGL_EXTRAS *ogl = (ALLEGRO_OGL_EXTRAS*)al_calloc(1, sizeof *ogl);
    display->ogl_extras = ogl;
    display->vt = _al_get_pandora_display_interface();
    display->flags = al_get_new_display_flags();
	#ifdef ALLEGRO_CFG_OPENGLES2
	display->flags |= ALLEGRO_PROGRAMMABLE_PIPELINE;
	#endif
printf("pandora_create_display(%d, %d)\n", w, h);
//    if (display->flags & ALLEGRO_FULLSCREEN_WINDOW) {
//        _al_raspberrypi_get_screen_size(adapter, &w, &h);
//    }

    ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();

    /* Add ourself to the list of displays. */
    ALLEGRO_DISPLAY_PANDORA **add;
    add = (ALLEGRO_DISPLAY_PANDORA **)_al_vector_alloc_back(&system->system.displays);
    *add = d;
    
    /* Each display is an event source. */
    _al_event_source_init(&display->es);

    display->extra_settings.settings[ALLEGRO_COMPATIBLE_DISPLAY] = 1;
//   ALLEGRO_EXTRA_DISPLAY_SETTINGS *eds[system->visuals_count];
//   memcpy(eds, system->visuals, sizeof(*eds) * system->visuals_count);
//   qsort(eds, system->visuals_count, sizeof(*eds), _al_display_settings_sorter);

//   ALLEGRO_INFO("Chose visual no. %i\n", eds[0]->index); 

//   memcpy(&display->extra_settings, eds[0], sizeof(ALLEGRO_EXTRA_DISPLAY_SETTINGS));

   display->w = w;
   display->h = h;

   mouse_scale_ratio_x = (float)display->w / 800;
   mouse_scale_ratio_y = (float)display->h / 480;


/*   if (EGL_Open(w, h)) {
      // FIXME: cleanup
      return NULL;
   }
*/   
   if (getenv("DISPLAY")) {
      _al_mutex_lock(&system->lock);
      Window root = RootWindow(
         system->x11display, DefaultScreen(system->x11display));
      XWindowAttributes attr;
      XGetWindowAttributes(system->x11display, root, &attr);
      d->window = XCreateWindow(
         system->x11display,
         root,
         0,
         0,
         attr.width,
         attr.height,
         0, 0,
         InputOnly,
         DefaultVisual(system->x11display, 0),
         0,
         NULL
      );
      XGetWindowAttributes(system->x11display, d->window, &attr);
      XSelectInput(
         system->x11display,
         d->window,
         PointerMotionMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask
      );
      XMapWindow(system->x11display, d->window);
      //XSetInputFocus(system->x11display, d->window, RevertToParent, CurrentTime);
      _al_xwin_reset_size_hints(display);
      _al_xwin_set_fullscreen_window(display, 2);
      _al_xwin_set_size_hints(display, INT_MAX, INT_MAX);
      d->wm_delete_window_atom = XInternAtom(system->x11display,
         "WM_DELETE_WINDOW", False);
      XSetWMProtocols(system->x11display, d->window, &d->wm_delete_window_atom, 1);
      _al_mutex_unlock(&system->lock);
   }

   if (EGL_Open(w, h)) {
	   return NULL;
   }
   al_grab_mouse(display);
   _al_ogl_manage_extensions(display);
   _al_ogl_set_extensions(ogl->extension_api);

   setup_gl(display);

   al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);
    
   display->flags |= ALLEGRO_OPENGL;

   d->cursor_hidden = false;

/*   if (al_is_mouse_installed() && !getenv("DISPLAY")) {
      _al_evdev_set_mouse_range(0, 0, display->w-1, display->h-1);
   }
*/
//   al_run_detached_thread(cursor_thread, display);
	// output some glInfo also
	const GLubyte* output;
	output = glGetString(GL_VENDOR);
	printf( "GL_VENDOR: %s\n", output);
	output = glGetString(GL_RENDERER);
	printf( "GL_RENDERER: %s\n", output);
	output = glGetString(GL_VERSION);
	printf( "GL_VERSION: %s\n", output);
	output = glGetString(GL_EXTENSIONS);
	printf( "GL_EXT: %s\n", output);
	
	#ifndef ALLEGRO_CFG_OPENGLES2
	// Create the glExtentions
	printf("Attaching glExtension...\n");
	glBlendEquation = (PFNGLBLENDEQUATIONOESPROC) eglGetProcAddress("glBlendEquationOES");
	if (glBlendEquation == NULL)
		printf("*** NO glBlendEquationOES found !!!\n");
	glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEOESPROC) eglGetProcAddress("glBlendFuncSeparateOES");
	if (glBlendFuncSeparate == NULL)
		printf("*** NO glBlendFuncSeparateOES found !!!\n");
    glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEOESPROC) eglGetProcAddress("glBlendEquationSeparateOES");
	if (glBlendEquationSeparate == NULL)
		printf("*** NO glBlendEquationSeparateOES found !!!\n");
    glGenerateMipmapEXT = (PFNGLGENERATEMIPMAPOESPROC) eglGetProcAddress("glGenerateMipmapOES");
	if (glGenerateMipmapEXT == NULL)
		printf("*** NO glGenerateMipmapOES found !!!\n");
    glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEROESPROC) eglGetProcAddress("glBindFramebufferOES");
	if (glBindFramebufferEXT == NULL)
		printf("*** NO glBindFramebufferOES found !!!\n");
    glDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSOESPROC) eglGetProcAddress("glDeleteFramebuffersOES");
	if (glDeleteFramebuffersEXT == NULL)
		printf("*** NO glDeleteFramebuffersOES found !!!\n");
	glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSOESPROC) eglGetProcAddress("glGenFramebuffersOES");
	if (glGenFramebuffersEXT == NULL)
		printf("*** NO glGenFramebuffersOES found !!!\n");
	glCheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSOESPROC) eglGetProcAddress("glCheckFramebufferStatusOES");
	if (glCheckFramebufferStatusEXT == NULL)
		printf("*** NO glCheckFramebufferStatusOES found !!!\n");
	glFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DOESPROC) eglGetProcAddress("glFramebufferTexture2DOES");
	if (glFramebufferTexture2DEXT == NULL)
		printf("*** NO glFramebufferTexture2DOES found !!!\n");
	glDrawTexiOES = (PFNGLDRAWTEXIOESPROC) eglGetProcAddress("glDrawTexiOES");
	if (glDrawTexiOES == NULL)
		printf("*** NO glDrawTexiOES found !!!\n");
	printf("Done with extensions\n");	
	#endif
	
   return display;
}

static void pandora_destroy_display(ALLEGRO_DISPLAY *d)
{
//printf("pandora_destroy_display(%x)\n", d);
   ALLEGRO_DISPLAY_PANDORA *pidisplay = (ALLEGRO_DISPLAY_PANDORA *)d;

//   stop_cursor_thread = true;

   _al_set_current_display_only(d);

   while (d->bitmaps._size > 0) {
      ALLEGRO_BITMAP **bptr = (ALLEGRO_BITMAP **)_al_vector_ref_back(&d->bitmaps);
      ALLEGRO_BITMAP *b = *bptr;
      _al_convert_to_memory_bitmap(b);
   }

   _al_event_source_free(&d->es);
   
   ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
   _al_vector_find_and_delete(&system->system.displays, &d);

   EGL_Close();

   if (getenv("DISPLAY")) {
      _al_mutex_lock(&system->lock);
      XUnmapWindow(system->x11display, pidisplay->window);
      XDestroyWindow(system->x11display, pidisplay->window);
      _al_mutex_unlock(&system->lock);
   }
   
   if (system->mouse_grab_display == d) {
      system->mouse_grab_display = NULL;
   }
   
}

static bool pandora_set_current_display(ALLEGRO_DISPLAY *d)
{
   (void)d;
// FIXME
   _al_ogl_update_render_state(d);
   return true;
}

void _al_pandora_get_mouse_scale_ratios(float *x, float *y)
{
	*x = mouse_scale_ratio_x;
	*y = mouse_scale_ratio_y;
}



static int pandora_get_orientation(ALLEGRO_DISPLAY *d)
{
   (void)d;
   return ALLEGRO_DISPLAY_ORIENTATION_0_DEGREES;
}


/* There can be only one window and only one OpenGL context, so all bitmaps
 * are compatible.
 */
static bool pandora_is_compatible_bitmap(ALLEGRO_DISPLAY *display,
                                      ALLEGRO_BITMAP *bitmap)
{
    (void)display;
    (void)bitmap;
    return true;
}

/* Resizing is not possible. */
static bool pandora_resize_display(ALLEGRO_DISPLAY *d, int w, int h)
{
    (void)d;
    (void)w;
    (void)h;
    return false;
}

/* The icon must be provided in the Info.plist file, it cannot be changed
 * at runtime.
 */
static void pandora_set_icons(ALLEGRO_DISPLAY *d, int num_icons, ALLEGRO_BITMAP *bitmaps[])
{
    (void)d;
    (void)num_icons;
    (void)bitmaps;
}

/* There is no way to leave fullscreen so no window title is visible. */
static void pandora_set_window_title(ALLEGRO_DISPLAY *display, char const *title)
{
    (void)display;
    (void)title;
}

/* The window always spans the entire screen right now. */
static void pandora_set_window_position(ALLEGRO_DISPLAY *display, int x, int y)
{
    (void)display;
    (void)x;
    (void)y;
}

/* The window cannot be constrained. */
static bool pandora_set_window_constraints(ALLEGRO_DISPLAY *display,
   int min_w, int min_h, int max_w, int max_h)
{
   (void)display;
   (void)min_w;
   (void)min_h;
   (void)max_w;
   (void)max_h;
   return false;
}

/* Always fullscreen. */
static bool pandora_set_display_flag(ALLEGRO_DISPLAY *display,
   int flag, bool onoff)
{
   (void)display;
   (void)flag;
   (void)onoff;
   return false;
}

static void pandora_get_window_position(ALLEGRO_DISPLAY *display, int *x, int *y)
{
    (void)display;
    *x = 0;
    *y = 0;
}

/* The window cannot be constrained. */
static bool pandora_get_window_constraints(ALLEGRO_DISPLAY *display,
   int *min_w, int *min_h, int *max_w, int *max_h)
{
   (void)display;
   (void)min_w;
   (void)min_h;
   (void)max_w;
   (void)max_h;
   return false;
}

static bool pandora_wait_for_vsync(ALLEGRO_DISPLAY *display)
{
    (void)display;
    Platform_VSync();
    return false;
}

void pandora_flip_display(ALLEGRO_DISPLAY *disp)
{
   (void)disp;
//   eglSwapBuffers(egl_display, egl_window);
	EGL_SwapBuffers();
}

static void pandora_update_display_region(ALLEGRO_DISPLAY *d, int x, int y,
                                       int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    pandora_flip_display(d);
}

static bool pandora_acknowledge_resize(ALLEGRO_DISPLAY *d)
{
//   _al_pandora_setup_opengl_view(d);
   (void)d;
   return true;
}

#ifndef USE_XCURSOR
static bool pandora_set_mouse_cursor(ALLEGRO_DISPLAY *display,
                                  ALLEGRO_MOUSE_CURSOR *cursor)
{
    (void)display;
    (void)cursor;
    return false;
}

static bool pandora_set_system_mouse_cursor(ALLEGRO_DISPLAY *display,
                                         ALLEGRO_SYSTEM_MOUSE_CURSOR cursor_id)
{
    (void)display;
    (void)cursor_id;
    return false;
}

static bool pandora_show_mouse_cursor(ALLEGRO_DISPLAY *display)
{
    ALLEGRO_DISPLAY_PANDORA *pando = (ALLEGRO_DISPLAY_PANDORA *)display;
    ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
    XFixesShowCursor(system->x11display, pando->window);
    return true;
}

static bool pandora_hide_mouse_cursor(ALLEGRO_DISPLAY *display)
{
    ALLEGRO_DISPLAY_PANDORA *pando = (ALLEGRO_DISPLAY_PANDORA *)display;
    ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
    XFixesHideCursor(system->x11display, pando->window);
    return true;
}
#endif
void _al_display_xglx_await_resize(ALLEGRO_DISPLAY *d, int old_resize_count,
   bool delay_hack)
{
   (void)d;
   (void)old_resize_count;
   (void)delay_hack;
}

/* Obtain a reference to this driver. */
ALLEGRO_DISPLAY_INTERFACE *_al_get_pandora_display_interface(void)
{
    if (vt)
        return vt;
    
    vt = al_calloc(1, sizeof *vt);
    
    vt->create_display = pandora_create_display;
    vt->destroy_display = pandora_destroy_display;
    vt->set_current_display = pandora_set_current_display;
    vt->flip_display = pandora_flip_display;
    vt->update_display_region = pandora_update_display_region;
    vt->acknowledge_resize = pandora_acknowledge_resize;
    vt->create_bitmap = _al_ogl_create_bitmap;
    vt->get_backbuffer = _al_ogl_get_backbuffer;
    vt->set_target_bitmap = _al_ogl_set_target_bitmap;
    
    vt->get_orientation = pandora_get_orientation;

    vt->is_compatible_bitmap = pandora_is_compatible_bitmap;
    vt->resize_display = pandora_resize_display;
    vt->set_icons = pandora_set_icons;
    vt->set_window_title = pandora_set_window_title;
    vt->set_window_position = pandora_set_window_position;
    vt->get_window_position = pandora_get_window_position;
    vt->set_window_constraints = pandora_set_window_constraints;
    vt->get_window_constraints = pandora_get_window_constraints;
    vt->set_display_flag = pandora_set_display_flag;
    vt->wait_for_vsync = pandora_wait_for_vsync;

    vt->update_render_state = _al_ogl_update_render_state;

    _al_ogl_add_drawing_functions(vt);

#ifndef USE_XCURSOR
    vt->set_mouse_cursor = pandora_set_mouse_cursor;
    vt->set_system_mouse_cursor = pandora_set_system_mouse_cursor;
    vt->show_mouse_cursor = pandora_show_mouse_cursor;
    vt->hide_mouse_cursor = pandora_hide_mouse_cursor;
#else
    _al_xwin_add_cursor_functions(vt);
#endif
    return vt;
}

/* GTK stuff */
static bool xdpy_create_display_hook_default(ALLEGRO_DISPLAY *display,
   int w, int h)
{
   ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
   ALLEGRO_DISPLAY_PANDORA *d = (ALLEGRO_DISPLAY_PANDORA *)display;
   (void)w;
   (void)h;

   XLockDisplay(system->x11display);

   XMapWindow(system->x11display, d->window);
   ALLEGRO_DEBUG("X11 window mapped.\n");

   d->wm_delete_window_atom = XInternAtom(system->x11display,
      "WM_DELETE_WINDOW", False);
   XSetWMProtocols(system->x11display, d->window, &d->wm_delete_window_atom, 1);

   XUnlockDisplay(system->x11display);

   d->overridable_vt = &default_overridable_vt;

   return true;
}

static void xdpy_destroy_display_hook_default(ALLEGRO_DISPLAY *d, bool is_last)
{
   ALLEGRO_SYSTEM_PANDORA *s = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
   ALLEGRO_DISPLAY_PANDORA *pando = (ALLEGRO_DISPLAY_PANDORA *)d;
   (void)is_last;

   EGL_Close();

   ALLEGRO_DEBUG("destroy window.\n");
   XDestroyWindow(s->x11display, pando->window);

}

static bool xdpy_resize_display_default(ALLEGRO_DISPLAY *d, int w, int h)
{
//   ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
//   ALLEGRO_DISPLAY_PANDORA *pando = (ALLEGRO_DISPLAY_PANDORA *)d;
   (void)d;
   (void)w;
   (void)h;
   return true;
}


static void xdpy_set_window_title_default(ALLEGRO_DISPLAY *display, const char *title)
{
   ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
   ALLEGRO_DISPLAY_PANDORA *pando = (ALLEGRO_DISPLAY_PANDORA *)display;

   {
      Atom WM_NAME = XInternAtom(system->x11display, "WM_NAME", False);
      Atom _NET_WM_NAME = XInternAtom(system->x11display, "_NET_WM_NAME", False);
      char *list[1] = { (char *) title };
      XTextProperty property;

      Xutf8TextListToTextProperty(system->x11display, list, 1, XUTF8StringStyle,
         &property);
      XSetTextProperty(system->x11display, pando->window, &property, WM_NAME);
      XSetTextProperty(system->x11display, pando->window, &property, _NET_WM_NAME);
      //XSetTextProperty(system->x11display, pando->window, &property, XA_WM_NAME);
      XFree(property.value);
   }
   {
      XClassHint *hint = XAllocClassHint();
      if (hint) {
         ALLEGRO_PATH *exepath = al_get_standard_path(ALLEGRO_EXENAME_PATH);
         // hint doesn't use a const char*, so we use strdup to create a non const string
         hint->res_name = strdup(al_get_path_basename(exepath));
         hint->res_class = strdup(al_get_path_basename(exepath));
         XSetClassHint(system->x11display, pando->window, hint);
         free(hint->res_name);
         free(hint->res_class);
         XFree(hint);
         al_destroy_path(exepath);
      }
   }
}

static void xdpy_set_fullscreen_window_default(ALLEGRO_DISPLAY *display, bool onoff)
{
   ALLEGRO_SYSTEM_PANDORA *system = (ALLEGRO_SYSTEM_PANDORA *)al_get_system_driver();
   if (onoff == !(display->flags & ALLEGRO_FULLSCREEN_WINDOW)) {
      _al_mutex_lock(&system->lock);
      _al_xwin_reset_size_hints(display);
      _al_xwin_set_fullscreen_window(display, 2);
      /* XXX Technically, the user may fiddle with the _NET_WM_STATE_FULLSCREEN
       * property outside of Allegro so this flag may not be in sync with
       * reality.
       */
      display->flags ^= ALLEGRO_FULLSCREEN_WINDOW;
      _al_xwin_set_size_hints(display, INT_MAX, INT_MAX);
      _al_mutex_unlock(&system->lock);
   }
}

static void xdpy_set_window_position_default(ALLEGRO_DISPLAY *display,
   int x, int y)
{
   ALLEGRO_DISPLAY_PANDORA *pando = (ALLEGRO_DISPLAY_PANDORA *)display;
   ALLEGRO_SYSTEM_PANDORA *system = (void *)al_get_system_driver();
   Window root, parent, child, *children;
   unsigned int n;

   _al_mutex_lock(&system->lock);

   /* To account for the window border, we have to find the parent window which
    * draws the border. If the parent is the root though, then we should not
    * translate.
    */
   XQueryTree(system->x11display, pando->window, &root, &parent, &children, &n);
   if (parent != root) {
      XTranslateCoordinates(system->x11display, parent, pando->window,
         x, y, &x, &y, &child);
   }

   XMoveWindow(system->x11display, pando->window, x, y);
   XFlush(system->x11display);

   /*pando->x = x;
   pando->y = y;*/

   _al_mutex_unlock(&system->lock);
}

static bool xdpy_set_window_constraints_default(ALLEGRO_DISPLAY *display,
   int min_w, int min_h, int max_w, int max_h)
{
   ALLEGRO_DISPLAY_PANDORA *pando = (ALLEGRO_DISPLAY_PANDORA *)display;

   pando->display.min_w = min_w;
   pando->display.min_h = min_h;
   pando->display.max_w = max_w;
   pando->display.max_h = max_h;

   int w = pando->display.w;
   int h = pando->display.h;
   int posX;
   int posY;

   if (min_w > 0 && w < min_w) {
      w = min_w;
   }
   if (min_h > 0 && h < min_h) {
      h = min_h;
   }
   if (max_w > 0 && w > max_w) {
      w = max_w;
   }
   if (max_h > 0 && h > max_h) {
      h = max_h;
   }

   al_get_window_position(display, &posX, &posY);
   _al_xwin_set_size_hints(display, posX, posY);
   /* Resize the display to its current size so constraints take effect. */
   al_resize_display(display, w, h);

   return true;
}

static const ALLEGRO_XWIN_DISPLAY_OVERRIDABLE_INTERFACE default_overridable_vt =
{
   xdpy_create_display_hook_default,
   xdpy_destroy_display_hook_default,
   xdpy_resize_display_default,
   xdpy_set_window_title_default,
   xdpy_set_fullscreen_window_default,
   xdpy_set_window_position_default,
   xdpy_set_window_constraints_default
};


bool _al_xwin_set_gtk_display_overridable_interface(uint32_t check_version,
   const ALLEGRO_XWIN_DISPLAY_OVERRIDABLE_INTERFACE *vt)
{
   /* The version of the native dialogs addon must exactly match the core
    * library version.
    */
   if (vt && check_version == ALLEGRO_VERSION_INT) {
      ALLEGRO_DEBUG("GTK vtable made available\n");
      gtk_override_vt = vt;
      return true;
   }

   ALLEGRO_DEBUG("GTK vtable reset\n");
   gtk_override_vt = NULL;
   return (vt == NULL);
}
