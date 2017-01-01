#include <stdio.h>
#include <X11/extensions/Xfixes.h>

#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"
#include "allegro5/internal/aintern_opengl.h"
#include "allegro5/internal/aintern_vector.h"
#include "allegro5/internal/aintern_odroid.h"
#include "allegro5/internal/aintern_x.h"
#include "allegro5/internal/aintern_xwindow.h"

ALLEGRO_DEBUG_CHANNEL("display")

#include "EGL/egl.h"

#define PITCH 128
#define DEFAULT_CURSOR_WIDTH 17
#define DEFAULT_CURSOR_HEIGHT 28

static ALLEGRO_DISPLAY_INTERFACE *vt;

struct ALLEGRO_DISPLAY_ODROID_EXTRA {
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

void _al_odroid_get_screen_info(int *dx, int *dy, int *screen_width, int *screen_height)
{
//printf("_al_odroid_get_screen_info(int *dx, int *dy, int *screen_width, int *screen_height)\n");
//   graphics_get_display_size(0 /* LCD */, (uint32_t *)screen_width, (uint32_t *)screen_height);
	// this is not right... better call egl get surface at least for the size of the width and height

   ALLEGRO_SYSTEM_ODROID *system = (ALLEGRO_SYSTEM_ODROID *)al_get_system_driver();
#if 1
   Window root_return;
   unsigned int border_width_return;
   unsigned int depth_return;
   XGetGeometry(system->x11display, RootWindow(system->x11display, DefaultScreen(system->x11display)), &root_return, 
	dx, dy, (unsigned int*)screen_width, (unsigned int*)screen_height, &border_width_return, &depth_return);
#else
   (*screen_width)=XWidthOfScreen(DefaultScreenOfDisplay(system->x11display));
   (*screen_height)=XHeightOfScreen(DefaultScreenOfDisplay(system->x11display));
   (*dx)=0;
   (*dy)=0;
#endif
printf("_al_odroid_get_screen_info(int *dx=%d, int *dy=%d, int *screen_width=%d, int *screen_height=%d)\n", *dx, *dy, *screen_width, *screen_height);
}

static ALLEGRO_DISPLAY *odroid_create_display(int w, int h)
{
    ALLEGRO_DISPLAY_ODROID *d = (ALLEGRO_DISPLAY_ODROID*)al_calloc(1, sizeof *d);
    ALLEGRO_DISPLAY *display = (ALLEGRO_DISPLAY*)d;
    ALLEGRO_OGL_EXTRAS *ogl = (ALLEGRO_OGL_EXTRAS*)al_calloc(1, sizeof *ogl);
    display->ogl_extras = ogl;
    display->vt = _al_get_odroid_display_interface();
    display->flags = al_get_new_display_flags();
	#ifdef ALLEGRO_CFG_OPENGLES2
	display->flags |= ALLEGRO_PROGRAMMABLE_PIPELINE;
	#endif
    if (display->flags & ALLEGRO_FULLSCREEN_WINDOW) {
	int dx, dy;
        _al_odroid_get_screen_info(&dx, &dy, &w, &h);
    }

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
#if 1
   /* Create a colormap. */
   Colormap cmap = XCreateColormap(system->x11display,
      DefaultRootWindow(system->x11display/*, DefaultScreenOfDisplay(system->x11display) d->xvinfo->screen*/),
      DefaultVisual(system->x11display, 0)/*d->xvinfo->visual*/, AllocNone);

   /* Create an X11 window */
   XSetWindowAttributes swa;
   int mask = CWBorderPixel | CWColormap | CWEventMask;
   swa.colormap = cmap;
   swa.border_pixel = 0;
   swa.event_mask =
      KeyPressMask |
      KeyReleaseMask |
      StructureNotifyMask |
      EnterWindowMask |
      LeaveWindowMask |
      FocusChangeMask |
      ExposureMask |
      PropertyChangeMask |
      ButtonPressMask |
      ButtonReleaseMask |
      PointerMotionMask;

   if (!(display->flags & ALLEGRO_RESIZABLE)) {
      mask |= CWBackPixel;
      swa.background_pixel = BlackPixel(system->x11display, DefaultScreen(system->x11display));
   }

   int x_off = INT_MAX;
   int y_off = INT_MAX;
   if (display->flags & ALLEGRO_FULLSCREEN) {
      //_al_xglx_get_display_offset(system, 0, &x_off, &y_off);
	x_off = 0;
	y_off = 0;
   }
   else {
      /* We want new_display_adapter's offset to add to the
       * new_window_position.
       */
      al_get_new_window_position(&x_off, &y_off);
   }

   d->window = XCreateWindow(system->x11display,
      RootWindow(system->x11display, DefaultScreen(system->x11display)),
      x_off != INT_MAX ? x_off : 0,
      y_off != INT_MAX ? y_off : 0,
      w, h, 0, DefaultDepth(system->x11display, DefaultScreen(system->x11display))/*d->xvinfo->depth*/,
      InputOutput, DefaultVisual(system->x11display, 0), mask, &swa);

   XMapWindow(system->x11display, d->window);
#else
   if (getenv("DISPLAY")) {
      _al_mutex_lock(&system->lock);
      Window root = DefaultRootWindow(
         system->x11display/*, DefaultScreen(system->x11display)*/);
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
   }
#endif

   printf("Window ID: %ld\n", (long)d->window);

   XSync(system->x11display, False);

   if ((display->flags & ALLEGRO_FULLSCREEN_WINDOW)) {
      ALLEGRO_INFO("Toggling fullscreen flag for %d x %d window.\n",
         display->w, display->h);
      _al_xwin_reset_size_hints(display);
      _al_xwin_set_fullscreen_window(display, 2);
      _al_xwin_set_size_hints(display, INT_MAX, INT_MAX);

      XWindowAttributes xwa;
      XGetWindowAttributes(system->x11display, d->window, &xwa);
      display->w = xwa.width;
      display->h = xwa.height;

      ALLEGRO_INFO("Using ALLEGRO_FULLSCREEN_WINDOW of %d x %d\n",
         display->w, display->h);
   }
   if (display->flags & ALLEGRO_FULLSCREEN) {
// Not supported for now, so same code as fullscreen_window...
      _al_xwin_reset_size_hints(display);
      _al_xwin_set_fullscreen_window(display, 2);
      _al_xwin_set_size_hints(display, INT_MAX, INT_MAX);
/*      d->wm_delete_window_atom = XInternAtom(system->x11display,
         "WM_DELETE_WINDOW", False);
      XSetWMProtocols(system->x11display, d->window, &d->wm_delete_window_atom, 1);*/
      XWindowAttributes xwa;
      XGetWindowAttributes(system->x11display, d->window, &xwa);
      display->w = xwa.width;
      display->h = xwa.height;
   }

   // EGL context creation
   static const EGLint contextAttribs[] =
   {
#if defined(ALLEGRO_CFG_OPENGLES2)
          EGL_CONTEXT_CLIENT_VERSION,     2,
#endif
          EGL_NONE
   };
   EGLint eglMajorVer, eglMinorVer;
   EGLBoolean result;
   const char* output;

   d->egldisplay = eglGetDisplay( (EGLNativeDisplayType) system->x11display );
   if (d->egldisplay == EGL_NO_DISPLAY)
   {
        printf( "ERROR: Unable to create EGL display.\n" );
   }
   result = eglInitialize( d->egldisplay, &eglMajorVer, &eglMinorVer );
   if (result != EGL_TRUE )
   {
        printf( "ERROR: Unable to initialize EGL display.\n" );
   }
   printf( "EGL Implementation Version: Major %d Minor %d\n", eglMajorVer, eglMinorVer );
   output = eglQueryString( d->egldisplay, EGL_VENDOR );
   printf( "EGL_VENDOR: %s\n", output );
   output = eglQueryString( d->egldisplay, EGL_VERSION );
   printf( "EGL_VERSION: %s\n", output );
   output = eglQueryString( d->egldisplay, EGL_EXTENSIONS );
   printf( "EGL_EXTENSIONS: %s\n", output );
   int attrib = 0;
   EGLint ConfigAttribs[23];
/*   ConfigAttribs[attrib++] = EGL_RED_SIZE;
   ConfigAttribs[attrib++] = 8;
   ConfigAttribs[attrib++] = EGL_GREEN_SIZE;
   ConfigAttribs[attrib++] = 8;
   ConfigAttribs[attrib++] = EGL_BLUE_SIZE;
   ConfigAttribs[attrib++] = 8;
   ConfigAttribs[attrib++] = EGL_DEPTH_SIZE;
   ConfigAttribs[attrib++] = 24;*/
 /*  ConfigAttribs[attrib++] = EGL_BUFFER_SIZE;
   ConfigAttribs[attrib++] = 32;*/
/*   ConfigAttribs[attrib++] = EGL_STENCIL_SIZE;
   ConfigAttribs[attrib++] = 8;*/
   ConfigAttribs[attrib++] = EGL_SURFACE_TYPE;
   ConfigAttribs[attrib++] = EGL_WINDOW_BIT;
   ConfigAttribs[attrib++] = EGL_RENDERABLE_TYPE;
#if !defined(ALLEGRO_CFG_OPENGLES2)
   ConfigAttribs[attrib++] = EGL_OPENGL_ES_BIT;
#else
   ConfigAttribs[attrib++] = EGL_OPENGL_ES2_BIT;
#endif
/*   ConfigAttribs[attrib++] = EGL_SAMPLE_BUFFERS;
   ConfigAttribs[attrib++] = 0;*/
   ConfigAttribs[attrib++] = EGL_NONE;

   int totalConfigsFound;
   EGLConfig eglconfig = 0;
   result = eglChooseConfig( d->egldisplay, ConfigAttribs, &eglconfig, 1, &totalConfigsFound );
   printf("there is %d config, result of eglChooseConfig is 0x%04X\n", totalConfigsFound, result);
#if defined(EGL_VERSION_1_2)
    /* Bind GLES and create the context */
    printf( "Binding API\n" );
    result = eglBindAPI( EGL_OPENGL_ES_API );
#endif /* EGL_VERSION_1_2 */
    printf( "Creating Context\n" );
    d->eglcontext = eglCreateContext( d->egldisplay, eglconfig, NULL, contextAttribs );
    if (d->eglcontext == EGL_NO_CONTEXT)
    {
        printf( "ERROR: Unable to create GLES context (0x%04X)\n", eglGetError());
    }
    printf( "Creating window surface\n" );

    d->eglwindow = eglCreateWindowSurface( d->egldisplay, eglconfig, d->window, 0 );
    if (d->eglwindow == EGL_NO_SURFACE)
    {
        printf( "ERROR: Unable to create EGL surface (0x%04X)!\n", eglGetError() );
    }

    printf( "EGLport: Making Current\n" );
    result = eglMakeCurrent( d->egldisplay,  d->eglwindow,  d->eglwindow, d->eglcontext );
    if (result != EGL_TRUE)
    {
        printf( "ERROR: Unable to make GLES context current (0x%04X)\n", eglGetError() );
    }
    printf( "GLES Setup Complete\n" );


   al_grab_mouse(display);
   _al_ogl_manage_extensions(display);
   _al_ogl_set_extensions(ogl->extension_api);

   setup_gl(display);

   // EGL context done


   _al_mutex_unlock(&system->lock);

   al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);
    
   display->flags |= ALLEGRO_OPENGL;

   d->cursor_hidden = false;

/*   if (al_is_mouse_installed() && !getenv("DISPLAY")) {
      _al_evdev_set_mouse_range(0, 0, display->w-1, display->h-1);
   }
*/
//   al_run_detached_thread(cursor_thread, display);
	// output some glInfo also
	output = (const char*)glGetString(GL_VENDOR);
	printf( "GL_VENDOR: %s\n", output);
	output = (const char*)glGetString(GL_RENDERER);
	printf( "GL_RENDERER: %s\n", output);
	output = (const char*)glGetString(GL_VERSION);
	printf( "GL_VERSION: %s\n", output);
	output = (const char*)glGetString(GL_EXTENSIONS);
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

static void odroid_destroy_display(ALLEGRO_DISPLAY *d)
{
//printf("odroid_destroy_display(%x)\n", d);
   ALLEGRO_DISPLAY_ODROID *odisplay = (ALLEGRO_DISPLAY_ODROID *)d;

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

    if (odisplay->egldisplay != NULL)
    {
        eglMakeCurrent( odisplay->egldisplay, NULL, NULL, EGL_NO_CONTEXT );
        if (odisplay->eglcontext != NULL) {
            eglDestroyContext( odisplay->egldisplay, odisplay->eglcontext );
        }
        if (odisplay->eglwindow != NULL) {
            eglDestroySurface( odisplay->egldisplay, odisplay->eglwindow );
        }
        eglTerminate( odisplay->egldisplay );
    }

    odisplay->eglwindow = NULL;
    odisplay->eglcontext = NULL;
    odisplay->egldisplay = NULL;

   if (getenv("DISPLAY")) {
      _al_mutex_lock(&system->lock);
      XUnmapWindow(system->x11display, odisplay->window);
      XDestroyWindow(system->x11display, odisplay->window);
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
    return false;
}

void odroid_flip_display(ALLEGRO_DISPLAY *disp)
{
    ALLEGRO_DISPLAY_ODROID *d = (ALLEGRO_DISPLAY_ODROID *)disp;
   eglSwapBuffers(d->egldisplay, d->eglwindow);
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
//   _al_odroid_setup_opengl_view(d);
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

void _al_display_xglx_await_resize(ALLEGRO_DISPLAY *d, int old_resize_count,
   bool delay_hack)
{
   ALLEGRO_SYSTEM_ODROID *system = (void *)al_get_system_driver();
   (void)d;
   (void)old_resize_count;

   ALLEGRO_DEBUG("Awaiting resize event\n");

   XSync(system->x11display, False);

   /* XXX: This hack helps when toggling between fullscreen windows and not,
    * on various window managers.
    */
   if (delay_hack) {
      al_rest(0.2);
   }
}

/* Obtain a reference to this driver. */
ALLEGRO_DISPLAY_INTERFACE *_al_get_odroid_display_interface(void)
{
    if (vt)
        return vt;
    
    vt = al_calloc(1, sizeof *vt);
    
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

    vt->update_render_state = _al_ogl_update_render_state;

    _al_ogl_add_drawing_functions(vt);

    vt->set_mouse_cursor = odroid_set_mouse_cursor;
    vt->set_system_mouse_cursor = odroid_set_system_mouse_cursor;
    vt->show_mouse_cursor = odroid_show_mouse_cursor;
    vt->hide_mouse_cursor = odroid_hide_mouse_cursor;
    
    return vt;
}
