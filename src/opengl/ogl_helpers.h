#ifndef __al_included_ogl_helpers_h
#define __al_included_ogl_helpers_h

#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"

/* Some definitions to smooth out the code in the opengl directory. */

#ifdef ALLEGRO_CFG_OPENGLES
   #define IS_OPENGLES        (true)
#else
   #define IS_OPENGLES        (false)
#endif

#ifdef ALLEGRO_IPHONE
   #define IS_IPHONE          (true)
#else
   #define IS_IPHONE          (false)
#endif

#ifdef ALLEGRO_ANDROID
   #define IS_ANDROID         (true)
   #define IS_ANDROID_AND(x)  (x)
#else
   #define IS_ANDROID         (false)
   #define IS_ANDROID_AND(x)  (false)
#endif

#ifdef ALLEGRO_RASPBERRYPI
   #define IS_RASPBERRYPI     (true)
#else
   #define IS_RASPBERRYPI     (false)
#endif

#ifdef ALLEGRO_PANDORA
   #define IS_PANDORA     (true)
#else
   #define IS_PANDORA     (false)
#endif

#if defined(ALLEGRO_ANDROID) || defined(ALLEGRO_RASPBERRYPI)
   #define UNLESS_ANDROID_OR_RPI(x) (0)
#else
   #define UNLESS_ANDROID_OR_RPI(x) (x)
#endif
#if defined(ALLEGRO_ANDROID) || defined(ALLEGRO_RASPBERRYPI) || defined(ALLEGRO_PANDORA)
   #define UNLESS_ANDROID_OR_RPI_OR_PANDORA(x) (0)
#else
   #define UNLESS_ANDROID_OR_RPI_OR_PANDORA(x) (x)
#endif

/* Android uses different functions/symbol names depending on ES version */
#define ANDROID_PROGRAMMABLE_PIPELINE(dpy) \
   IS_ANDROID_AND(al_get_display_flags(dpy) & ALLEGRO_PROGRAMMABLE_PIPELINE)

/* XXX still hacky */
#if defined ALLEGRO_RASPBERRYPI
   #define GL_COLOR_ATTACHMENT0_EXT    GL_COLOR_ATTACHMENT0
   #define GL_FRAMEBUFFER_BINDING_EXT  GL_FRAMEBUFFER_BINDING
   #define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
   #define GL_FRAMEBUFFER_EXT          GL_FRAMEBUFFER
   #define glBindFramebufferEXT        glBindFramebuffer
   #define glCheckFramebufferStatusEXT glCheckFramebufferStatus
   #define glDeleteFramebuffersEXT     glDeleteFramebuffers
   #define glFramebufferTexture2DEXT   glFramebufferTexture2D
   #define glGenFramebuffersEXT        glGenFramebuffers
   #define glGenerateMipmapEXT         glGenerateMipmap
   #define glOrtho                     glOrthof
#elif defined ALLEGRO_PANDORA
   #ifdef ALLEGRO_CFG_NO_GLES2
   #define GL_COLOR_ATTACHMENT0_EXT GL_COLOR_ATTACHMENT0_OES
   #define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE_OES
   #define GL_INVALID_FRAMEBUFFER_OPERATION GL_INVALID_FRAMEBUFFER_OPERATION_OES
   #else
   #define GL_COLOR_ATTACHMENT0_EXT    GL_COLOR_ATTACHMENT0
   #define GL_FRAMEBUFFER_BINDING_EXT  GL_FRAMEBUFFER_BINDING
   #define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
   #define GL_FRAMEBUFFER_EXT          GL_FRAMEBUFFER
   #define glBindFramebufferEXT        glBindFramebuffer
   #define glCheckFramebufferStatusEXT glCheckFramebufferStatus
   #define glDeleteFramebuffersEXT     glDeleteFramebuffers
   #define glFramebufferTexture2DEXT   glFramebufferTexture2D
   #define glGenFramebuffersEXT        glGenFramebuffers
   #define glGenerateMipmapEXT         glGenerateMipmap
   #endif
   #define glOrtho glOrthof
#elif defined ALLEGRO_CFG_OPENGLES
   #define GL_COLOR_ATTACHMENT0_EXT    GL_COLOR_ATTACHMENT0_OES
   #define GL_FRAMEBUFFER_BINDING_EXT  GL_FRAMEBUFFER_BINDING_OES
   #define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE_OES
   #define GL_FRAMEBUFFER_EXT          GL_FRAMEBUFFER_OES
   #define glBindFramebufferEXT        glBindFramebufferOES
   #define glCheckFramebufferStatusEXT glCheckFramebufferStatusOES
   #define glDeleteFramebuffersEXT     glDeleteFramebuffersOES
   #define glFramebufferTexture2DEXT   glFramebufferTexture2DOES
   #define glGenFramebuffersEXT        glGenFramebuffersOES
   #define glGenerateMipmapEXT         glGenerateMipmapOES
   #define glOrtho                     glOrthof
#endif

#endif

/* vim: set sts=3 sw=3 et: */
