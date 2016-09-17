#include <stdio.h>
#include <X11/extensions/Xfixes.h>

#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"
#include "allegro5/internal/aintern_iphone.h"
#include "allegro5/internal/aintern_opengl.h"
#include "allegro5/internal/aintern_vector.h"
#include "allegro5/internal/aintern_odroid.h"
#include "allegro5/internal/aintern_x.h"
#include "allegro5/internal/aintern_xwindow.h"

#define ODROID
//#define USE_EGL_RAW
#ifdef ALLEGRO_CFG_NO_GLES2
#define USE_GLES1
#else
#define USE_GLES2
#endif
#include "eglport.h"

#define PITCH 128
#define DEFAULT_CURSOR_WIDTH 17
#define DEFAULT_CURSOR_HEIGHT 28


static ALLEGRO_DISPLAY_INTERFACE *vt;

struct ALLEGRO_DISPLAY_ODROID_EXTRA {
};

void _al_odroid_setup_opengl_view(ALLEGRO_DISPLAY *d)
{
//printf("_al_odroid_setup_opengl_view(%x)\n", d);
   glViewport(0, 0, d->w, d->h);

   al_identity_transform(&d->proj_transform);
   al_orthographic_transform(&d->proj_transform, 0, 0, -1, d->w, d->h, 1);

   al_identity_transform(&d->view_transform);
#ifdef ALLEGRO_CFG_NO_GLES2
   if (!(d->flags & ALLEGRO_PROGRAMMABLE_PIPELINE)) {
      glMatrixMode(GL_PROJECTION);
      glLoadMatrixf((float *)d->proj_transform.m);
      glMatrixMode(GL_MODELVIEW);
      glLoadMatrixf((float *)d->view_transform.m);
   }
#endif
}

/* Helper to set up GL state as we want it. */
static void setup_gl(ALLEGRO_DISPLAY *d)
{
//printf("setup_gl(%x)\n", d);
    ALLEGRO_OGL_EXTRAS *ogl = d->ogl_extras;

    if (ogl->backbuffer)
        _al_ogl_resize_backbuffer(ogl->backbuffer, d->w, d->h);
    else
        ogl->backbuffer = _al_ogl_create_backbuffer(d);

    _al_odroid_setup_opengl_view(d);
}

void _al_odroid_get_screen_info(int *dx, int *dy, int *screen_width, int *screen_height)
{
//printf("_al_odroid_get_screen_info(int *dx, int *dy, int *screen_width, int *screen_height)\n");
   graphics_get_display_size(0 /* LCD */, (uint32_t *)screen_width, (uint32_t *)screen_height);
//   (*screen_width)=800;
//   (*screen_height)=480;
   (*dx)=0;
   (*dy)=0;
}

static ALLEGRO_DISPLAY *odroid_create_display(int w, int h)
{
    ALLEGRO_DISPLAY_ODROID *d = (ALLEGRO_DISPLAY_ODROID*)al_calloc(1, sizeof *d);
    ALLEGRO_DISPLAY *display = (ALLEGRO_DISPLAY*)d;
    ALLEGRO_OGL_EXTRAS *ogl = (ALLEGRO_OGL_EXTRAS*)al_calloc(1, sizeof *ogl);
    display->ogl_extras = ogl;
    display->vt = _al_get_odroid_display_interface();
    display->flags = al_get_new_display_flags();
	#ifndef ALLEGRO_CFG_NO_GLES2
	display->flags |= ALLEGRO_PROGRAMMABLE_PIPELINE;
	#endif
//    if (display->flags & ALLEGRO_FULLSCREEN_WINDOW) {
//        _al_raspberrypi_get_screen_size(adapter, &w, &h);
//    }

    ALLEGRO_SYSTEM_ODROID *system = (ALLEGRO_SYSTEM_ODROID *)al_get_system_driver();

    /* Add ourself to the list of displays. */
    ALLEGRO_DISPLAY_ODROID **add;
    add = (ALLEGRO_DISPLAY_ODROID **)_al_vector_alloc_back(&system->system.displays);
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
	
	#ifdef ALLEGRO_CFG_NO_GLES2
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

static void odroid_destroy_display(ALLEGRO_DISPLAY *d)
{
//printf("odroid_destroy_display(%x)\n", d);
   ALLEGRO_DISPLAY_ODROID *pidisplay = (ALLEGRO_DISPLAY_ODROID *)d;

//   stop_cursor_thread = true;

   _al_set_current_display_only(d);

   while (d->bitmaps._size > 0) {
      ALLEGRO_BITMAP **bptr = (ALLEGRO_BITMAP **)_al_vector_ref_back(&d->bitmaps);
      ALLEGRO_BITMAP *b = *bptr;
      _al_convert_to_memory_bitmap(b);
   }

   _al_event_source_free(&d->es);
   
   ALLEGRO_SYSTEM_ODROID *system = (ALLEGRO_SYSTEM_ODROID *)al_get_system_driver();
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

static bool odroid_set_current_display(ALLEGRO_DISPLAY *d)
{
   (void)d;
// FIXME
   _al_ogl_update_render_state(d);
   return true;
}

static int odroid_get_orientation(ALLEGRO_DISPLAY *d)
{
   (void)d;
   return ALLEGRO_DISPLAY_ORIENTATION_0_DEGREES;
}


/* There can be only one window and only one OpenGL context, so all bitmaps
 * are compatible.
 */
static bool odroid_is_compatible_bitmap(ALLEGRO_DISPLAY *display,
                                      ALLEGRO_BITMAP *bitmap)
{
    (void)display;
    (void)bitmap;
    return true;
}

/* Resizing is not possible. */
static bool odroid_resize_display(ALLEGRO_DISPLAY *d, int w, int h)
{
    (void)d;
    (void)w;
    (void)h;
    return false;
}

/* The icon must be provided in the Info.plist file, it cannot be changed
 * at runtime.
 */
static void odroid_set_icons(ALLEGRO_DISPLAY *d, int num_icons, ALLEGRO_BITMAP *bitmaps[])
{
    (void)d;
    (void)num_icons;
    (void)bitmaps;
}

/* There is no way to leave fullscreen so no window title is visible. */
static void odroid_set_window_title(ALLEGRO_DISPLAY *display, char const *title)
{
    (void)display;
    (void)title;
}

/* The window always spans the entire screen right now. */
static void odroid_set_window_position(ALLEGRO_DISPLAY *display, int x, int y)
{
    (void)display;
    (void)x;
    (void)y;
}

/* The window cannot be constrained. */
static bool odroid_set_window_constraints(ALLEGRO_DISPLAY *display,
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
static bool odroid_set_display_flag(ALLEGRO_DISPLAY *display,
   int flag, bool onoff)
{
   (void)display;
   (void)flag;
   (void)onoff;
   return false;
}

static void odroid_get_window_position(ALLEGRO_DISPLAY *display, int *x, int *y)
{
    (void)display;
    *x = 0;
    *y = 0;
}

/* The window cannot be constrained. */
static bool odroid_get_window_constraints(ALLEGRO_DISPLAY *display,
   int *min_w, int *min_h, int *max_w, int *max_h)
{
   (void)display;
   (void)min_w;
   (void)min_h;
   (void)max_w;
   (void)max_h;
   return false;
}

static bool odroid_wait_for_vsync(ALLEGRO_DISPLAY *display)
{
    (void)display;
    Platform_VSync();
    return false;
}

void odroid_flip_display(ALLEGRO_DISPLAY *disp)
{
   (void)disp;
//   eglSwapBuffers(egl_display, egl_window);
	EGL_SwapBuffers();
}

static void odroid_update_display_region(ALLEGRO_DISPLAY *d, int x, int y,
                                       int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    odroid_flip_display(d);
}

static bool odroid_acknowledge_resize(ALLEGRO_DISPLAY *d)
{
   _al_odroid_setup_opengl_view(d);
   return true;
}

static bool odroid_set_mouse_cursor(ALLEGRO_DISPLAY *display,
                                  ALLEGRO_MOUSE_CURSOR *cursor)
{
    (void)display;
    (void)cursor;
    return false;
}

static bool odroid_set_system_mouse_cursor(ALLEGRO_DISPLAY *display,
                                         ALLEGRO_SYSTEM_MOUSE_CURSOR cursor_id)
{
    (void)display;
    (void)cursor_id;
    return false;
}

static bool odroid_show_mouse_cursor(ALLEGRO_DISPLAY *display)
{
    ALLEGRO_DISPLAY_ODROID *pidisplay = (ALLEGRO_DISPLAY_ODROID *)display;
    ALLEGRO_SYSTEM_ODROID *system = (ALLEGRO_SYSTEM_ODROID *)al_get_system_driver();
    XFixesShowCursor(system->x11display, pidisplay->window);
    return true;
}

static bool odroid_hide_mouse_cursor(ALLEGRO_DISPLAY *display)
{
    ALLEGRO_DISPLAY_ODROID *pidisplay = (ALLEGRO_DISPLAY_ODROID *)display;
    ALLEGRO_SYSTEM_ODROID *system = (ALLEGRO_SYSTEM_ODROID *)al_get_system_driver();
    XFixesHideCursor(system->x11display, pidisplay->window);
    return true;
}

/* Obtain a reference to this driver. */
ALLEGRO_DISPLAY_INTERFACE *_al_get_odroid_display_interface(void)
{
    if (vt)
        return vt;
    
    vt = (ALLEGRO_DISPLAY_INTERFACE*)al_calloc(1, sizeof *vt);
    
    vt->create_display = odroid_create_display;
    vt->destroy_display = odroid_destroy_display;
    vt->set_current_display = odroid_set_current_display;
    vt->flip_display = odroid_flip_display;
    vt->update_display_region = odroid_update_display_region;
    vt->acknowledge_resize = odroid_acknowledge_resize;
    vt->create_bitmap = _al_ogl_create_bitmap;
    vt->get_backbuffer = _al_ogl_get_backbuffer;
    vt->set_target_bitmap = _al_ogl_set_target_bitmap;
    
    vt->get_orientation = odroid_get_orientation;

    vt->is_compatible_bitmap = odroid_is_compatible_bitmap;
    vt->resize_display = odroid_resize_display;
    vt->set_icons = odroid_set_icons;
    vt->set_window_title = odroid_set_window_title;
    vt->set_window_position = odroid_set_window_position;
    vt->get_window_position = odroid_get_window_position;
    vt->set_window_constraints = odroid_set_window_constraints;
    vt->get_window_constraints = odroid_get_window_constraints;
    vt->set_display_flag = odroid_set_display_flag;
    vt->wait_for_vsync = odroid_wait_for_vsync;


    //vt->acknowledge_drawing_halt = _al_raspberrypi_acknowledge_drawing_halt;
    vt->update_render_state = _al_ogl_update_render_state;

    _al_ogl_add_drawing_functions(vt);
    // stub mouse function
    // add default X mouse functions
//    _al_xwin_add_cursor_functions(vt);
    vt->set_mouse_cursor = odroid_set_mouse_cursor;
    vt->set_system_mouse_cursor = odroid_set_system_mouse_cursor;
	// overload show/hide functions
    vt->show_mouse_cursor = odroid_show_mouse_cursor;
    vt->hide_mouse_cursor = odroid_hide_mouse_cursor;
    
    return vt;
}
