/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      OpenGL bitmap locking (GLES).
 *
 *      See LICENSE.txt for copyright information.
 */

#include <stdio.h>

#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_opengl.h"
#include "allegro5/internal/aintern_pixels.h"

#if defined ALLEGRO_ANDROID
   #include "allegro5/internal/aintern_android.h"
#endif

#include "ogl_helpers.h"

/*
 * This is the "old" implementation of ogl_lock_region and ogl_unlock_region,
 * still used for GLES.  The refactored version is in ogl_lock.c.
 */
#if defined(ALLEGRO_CFG_OPENGLES)

ALLEGRO_DEBUG_CHANNEL("opengl")

static void ogl_unlock_region_old_gles_backbuffer(ALLEGRO_BITMAP *bitmap);
static void ogl_unlock_region_old_rpi_backbuffer(ALLEGRO_BITMAP *bitmap,
   ALLEGRO_DISPLAY *disp);


/* Helper to get smallest fitting power of two. */
static int pot(int x)
{
   int y = 1;
   while (y < x) y *= 2;
   return y;
}


static int ogl_pixel_alignment(int pixel_size)
{
   /* Valid alignments are: 1, 2, 4, 8 bytes. */
   switch (pixel_size) {
      case 1:
      case 2:
      case 4:
      case 8:
         return pixel_size;
      case 3:
         return 1;
      case 16: /* float32 */
         return 4;
      default:
         ASSERT(false);
         return 4;
   }
}


static int ogl_pitch(int w, int pixel_size)
{
   int pitch = w * pixel_size;
   return pitch;
}


/* OpenGL cannot "lock" pixels, so instead we update our memory copy and
 * return a pointer into that.
 */
ALLEGRO_LOCKED_REGION *_al_ogl_lock_region_old_gles(ALLEGRO_BITMAP *bitmap,
   int x, int y, int w, int h, int format, int flags)
{
   ALLEGRO_BITMAP_EXTRA_OPENGL *ogl_bitmap = bitmap->extra;
   int pixel_size;
   int pixel_alignment;
   int pitch;
   ALLEGRO_DISPLAY *disp;
   ALLEGRO_DISPLAY *old_disp = NULL;
   ALLEGRO_BITMAP *old_target = NULL;
   bool need_to_restore_target = false;
   bool need_to_restore_display = false;
   GLint gl_y = bitmap->h - y - h;
   GLenum e;

   if (format == ALLEGRO_PIXEL_FORMAT_ANY)
      format = bitmap->format;

   disp = al_get_current_display();
   format = _al_get_real_pixel_format(disp, format);

   pixel_size = al_get_pixel_size(format);
   pixel_alignment = ogl_pixel_alignment(pixel_size);

   if (!disp || (bitmap->display->ogl_extras->is_shared == false &&
         bitmap->display != disp)) {
      need_to_restore_display = true;
      old_disp = disp;
      _al_set_current_display_only(bitmap->display);
   }

   /* Set up the pixel store state.  We will need to match it when unlocking.
    * There may be other pixel store state we should be setting.
    * See also pitfalls 7 & 8 from:
    * http://www.opengl.org/resources/features/KilgardTechniques/oglpitfall/
    */

   glPixelStorei(GL_PACK_ALIGNMENT, pixel_alignment);
   e = glGetError();
   if (e) {
      ALLEGRO_ERROR("glPixelStorei(GL_PACK_ALIGNMENT, %d) failed (%s).\n",
         pixel_alignment, _al_gl_error_string(e));
   }

   if (ogl_bitmap->is_backbuffer) {
      ALLEGRO_DEBUG("Locking backbuffer\n");

      pitch = ogl_pitch(w, pixel_size);
      ogl_bitmap->lock_buffer = al_malloc(pitch * h);

      if (!(flags & ALLEGRO_LOCK_WRITEONLY)) {
         glReadPixels(x, gl_y, w, h,
            _al_ogl_get_glformat(format, 2),
            _al_ogl_get_glformat(format, 1),
            ogl_bitmap->lock_buffer);
         e = glGetError();
         if (e) {
            ALLEGRO_ERROR("glReadPixels for format %s failed (%s).\n",
               _al_pixel_format_name(format), _al_gl_error_string(e));
         }
      }

      bitmap->locked_region.data = ogl_bitmap->lock_buffer +
         pitch * (h - 1);
   }
   else {
      if (flags & ALLEGRO_LOCK_WRITEONLY) {
         ALLEGRO_DEBUG("Locking non-backbuffer WRITEONLY\n");

         pitch = w * pixel_size;

         ogl_bitmap->lock_buffer = al_malloc(pitch * h);
         bitmap->locked_region.data = ogl_bitmap->lock_buffer +
            pitch * (h - 1);

         // Not sure why this is needed on Pi
         if (IS_RASPBERRYPI) {
            glFlush();
         }
      }
      else {
         ALLEGRO_DEBUG("Locking non-backbuffer %s\n",
            flags & ALLEGRO_LOCK_READONLY ? "READONLY" : "READWRITE");

         /* Create an FBO if there isn't one. */
         if (!ogl_bitmap->fbo_info) {
            old_target = al_get_target_bitmap();
            need_to_restore_target = true;
            bitmap->locked = false; // FIXME: hack :(
            if (al_is_bitmap_drawing_held())
               al_hold_bitmap_drawing(false);

            al_set_target_bitmap(bitmap); // This creates the fbo
            bitmap->locked = true;
         }

         if (ogl_bitmap->fbo_info) {
            GLint old_fbo;
            int start_h;

            old_fbo = _al_ogl_bind_framebuffer(ogl_bitmap->fbo_info->fbo);

            e = glGetError();
            if (e) {
               ALLEGRO_ERROR("glBindFramebufferEXT failed (%s).\n",
                  _al_gl_error_string(e));
            }

            start_h = h;

            /* Some Android devices only want to read POT chunks with glReadPixels.
             * This adds yet more overhead, but AFAICT it fails any other way.
             * Test device was gen 1 Galaxy Tab. Also read 16x16 minimum.
             */
            if (IS_ANDROID || IS_PANDORA || IS_ODROID) {
               glPixelStorei(GL_PACK_ALIGNMENT, 4);
               w = pot(w);
               while (w < 16) w = pot(w+1);
               h = pot(h);
               while (h < 16) h = pot(h+1);
            }

            pitch = ogl_pitch(w, pixel_size);
            /* Allocate a buffer big enough for both purposes. This requires more
             * memory to be held for the period of the lock, but overall less
             * memory is needed to complete the lock.
             */
            ogl_bitmap->lock_buffer = al_malloc(_ALLEGRO_MAX(pitch * h, ogl_pitch(w, 4) * h));

            /* NOTE: GLES (1.1?) can only read 4 byte pixels, we have to convert */
            glReadPixels(x, gl_y, w, h,
               GL_RGBA, GL_UNSIGNED_BYTE,
               ogl_bitmap->lock_buffer);
            e = glGetError();
            if (e) {
               ALLEGRO_ERROR("glReadPixels for format %s failed (%s).\n",
                  _al_pixel_format_name(format), _al_gl_error_string(e));
            }

            /* That's right, we convert in-place.
             * (safe as long as dst size <= src size, which it always is)
             */
            _al_convert_bitmap_data(
               ogl_bitmap->lock_buffer,
               ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
               ogl_pitch(w, 4),
               ogl_bitmap->lock_buffer,
               format,
               pitch,
               0, 0, 0, 0,
               w, h);

            bitmap->locked_region.data =
               ogl_bitmap->lock_buffer + pitch * (start_h - 1);

            _al_ogl_bind_framebuffer(old_fbo);
         }
         else {
            /* Unreachable on Android. (OpenGLES?) */
            abort();
         }
      }
   }

   bitmap->locked_region.format = format;
   bitmap->locked_region.pitch = -pitch;
   bitmap->locked_region.pixel_size = pixel_size;

   if (need_to_restore_target)
      al_set_target_bitmap(old_target);

   if (need_to_restore_display) {
      _al_set_current_display_only(old_disp);
   }

   return &bitmap->locked_region;
}


/* Synchronizes the texture back to the (possibly modified) bitmap data.
 */
void _al_ogl_unlock_region_old_gles(ALLEGRO_BITMAP *bitmap)
{
   ALLEGRO_BITMAP_EXTRA_OPENGL *ogl_bitmap = bitmap->extra;
   const int lock_format = bitmap->locked_region.format;
   ALLEGRO_DISPLAY *disp;
   ALLEGRO_DISPLAY *old_disp = NULL;
   GLenum e;
   int w = bitmap->lock_w;
   int h = bitmap->lock_h;
   GLint gl_y = bitmap->h - bitmap->lock_y - h;
   int orig_format;
   int orig_pixel_size;
   int pixel_alignment;
   (void)e;

   if (bitmap->lock_flags & ALLEGRO_LOCK_READONLY) {
      ALLEGRO_DEBUG("Unlocking non-backbuffer READONLY\n");
      goto Done;
   }

   disp = al_get_current_display();
   orig_format = _al_get_real_pixel_format(disp, bitmap->format);

   if (!disp || (bitmap->display->ogl_extras->is_shared == false &&
       bitmap->display != disp)) {
      old_disp = disp;
      _al_set_current_display_only(bitmap->display);
   }

   orig_pixel_size = al_get_pixel_size(orig_format);

   if (ogl_bitmap->is_backbuffer) {
      ALLEGRO_DEBUG("Unlocking backbuffer\n");
#ifndef ALLEGRO_NO_GLES1
      if ((IS_RASPBERRYPI) /*|| (IS_PANDORA)*/)
#endif
         ogl_unlock_region_old_rpi_backbuffer(bitmap, disp);
#ifndef ALLEGRO_NO_GLES1
      else
         ogl_unlock_region_old_gles_backbuffer(bitmap);
#endif
   }
   else {
      GLint fbo;
#ifdef ALLEGRO_ANDROID
      fbo = _al_android_get_curr_fbo();
#else
      glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &fbo);
#endif
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#ifdef ALLEGRO_ANDROID
      _al_android_set_curr_fbo(0);
#endif

      glBindTexture(GL_TEXTURE_2D, ogl_bitmap->texture);

      if ((IS_ANDROID || IS_PANDORA || IS_ODROID) && !(bitmap->lock_flags & ALLEGRO_LOCK_WRITEONLY)) {
         w = pot(w);
      }

      if (!ogl_bitmap->fbo_info ||
            (bitmap->locked_region.format != orig_format)) {

         int dst_pitch = w * orig_pixel_size;
         unsigned char *tmpbuf = al_malloc(dst_pitch * h);

         ALLEGRO_DEBUG("Unlocking non-backbuffer with conversion\n");

         _al_convert_bitmap_data(
            ogl_bitmap->lock_buffer,
            bitmap->locked_region.format,
            -bitmap->locked_region.pitch,
            tmpbuf,
            orig_format,
            dst_pitch,
            0, 0, 0, 0,
            w, h);

         pixel_alignment = ogl_pixel_alignment(orig_pixel_size);
         glPixelStorei(GL_UNPACK_ALIGNMENT, pixel_alignment);

         glTexSubImage2D(GL_TEXTURE_2D, 0,
            bitmap->lock_x, gl_y,
            w, h,
            _al_ogl_get_glformat(orig_format, 2),
            _al_ogl_get_glformat(orig_format, 1),
            tmpbuf);
         al_free(tmpbuf);
         e = glGetError();
         if (e) {
            ALLEGRO_ERROR("glTexSubImage2D for format %d failed (%s).\n",
               lock_format, _al_gl_error_string(e));
         }
      }
      else {
         ALLEGRO_DEBUG("Unlocking non-backbuffer without conversion\n");

         pixel_alignment = ogl_pixel_alignment(orig_pixel_size);
         glPixelStorei(GL_UNPACK_ALIGNMENT, pixel_alignment);

         glTexSubImage2D(GL_TEXTURE_2D, 0, bitmap->lock_x, gl_y,
            w, h,
            _al_ogl_get_glformat(lock_format, 2),
            _al_ogl_get_glformat(lock_format, 1),
            ogl_bitmap->lock_buffer);
         e = glGetError();
         if (e) {
            GLint tex_internalformat;
            (void)tex_internalformat;
            ALLEGRO_ERROR("glTexSubImage2D for format %s failed (%s).\n",
               _al_pixel_format_name(lock_format), _al_gl_error_string(e));
         }
      }

      if (bitmap->flags & ALLEGRO_MIPMAP) {
         /* If using FBOs, we need to regenerate mipmaps explicitly now. */
         if (al_get_opengl_extension_list()->ALLEGRO_GL_EXT_framebuffer_object) {
            glGenerateMipmapEXT(GL_TEXTURE_2D);
            e = glGetError();
            if (e) {
               ALLEGRO_ERROR("glGenerateMipmapEXT for texture %d failed (%s).\n",
                  ogl_bitmap->texture, _al_gl_error_string(e));
            }
         }
      }

      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
#ifdef ALLEGRO_ANDROID
      _al_android_set_curr_fbo(fbo);
#endif
   }

   if (old_disp) {
      _al_set_current_display_only(old_disp);
   }

Done:

   al_free(ogl_bitmap->lock_buffer);
   ogl_bitmap->lock_buffer = NULL;
}

#ifndef ALLEGRO_NO_GLES1
static void ogl_unlock_region_old_gles_backbuffer(ALLEGRO_BITMAP *bitmap)
{
   ALLEGRO_BITMAP_EXTRA_OPENGL *ogl_bitmap = bitmap->extra;
   const int lock_format = bitmap->locked_region.format;
   GLuint tmp_tex;
   int e;

   glGenTextures(1, &tmp_tex);
   glBindTexture(GL_TEXTURE_2D, tmp_tex);
   #ifdef ALLEGRO_PANDORA
 printf("glTexImage2D(GL_TEXTURE_2D, %i, 0x%x, %i, %i, %i, 0x%x, 0x%x, %p)\n", 0, _al_ogl_get_glformat(lock_format, 2),
      bitmap->lock_w, bitmap->lock_h,
      0, _al_ogl_get_glformat(lock_format, 2), _al_ogl_get_glformat(lock_format, 1),
      ogl_bitmap->lock_buffer);
   glTexImage2D(GL_TEXTURE_2D, 0, _al_ogl_get_glformat(lock_format, 2),
      pot(bitmap->lock_w), pot(bitmap->lock_h),
      0, _al_ogl_get_glformat(lock_format, 2), _al_ogl_get_glformat(lock_format, 1),
      0);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 
      0, 0, 
      bitmap->lock_w, bitmap->lock_h,
      _al_ogl_get_glformat(lock_format, 2), _al_ogl_get_glformat(lock_format, 1),
      ogl_bitmap->lock_buffer);
   #else
   glTexImage2D(GL_TEXTURE_2D, 0, _al_ogl_get_glformat(lock_format, 0),
      bitmap->lock_w, bitmap->lock_h,
      0, _al_ogl_get_glformat(lock_format, 2), _al_ogl_get_glformat(lock_format, 1),
      ogl_bitmap->lock_buffer);
   #endif
   e = glGetError();
   if (e) {
      printf("glTexImage2D failed: %d\n", e);
   }

   glDrawTexiOES(bitmap->lock_x, bitmap->lock_y, 0, bitmap->lock_w,
      bitmap->lock_h);
   e = glGetError();
   if (e) {
      printf("glDrawTexiOES failed: %d\n", e);
   }

   glDeleteTextures(1, &tmp_tex);
}
#endif

static void ogl_unlock_region_old_rpi_backbuffer(ALLEGRO_BITMAP *bitmap,
   ALLEGRO_DISPLAY *disp)
{
   ALLEGRO_BITMAP_EXTRA_OPENGL *ogl_bitmap = bitmap->extra;
   const int lock_format = bitmap->locked_region.format;
   ALLEGRO_BITMAP *start_target = al_get_target_bitmap();
   const bool already_is_target = (start_target == bitmap);
   ALLEGRO_STATE st;

   al_store_state(&st,
      ALLEGRO_STATE_NEW_BITMAP_PARAMETERS |
      (already_is_target ? 0 : ALLEGRO_STATE_TARGET_BITMAP));
   al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP);

   ALLEGRO_TRANSFORM t, backup1, backup2;
   al_copy_transform(&backup1, al_get_projection_transform(disp));
   al_copy_transform(&backup2, al_get_current_transform());
   al_identity_transform(&t);
   al_use_transform(&t);
   al_orthographic_transform(&t, 0, 0, -1, disp->w, disp->h, 1);
   al_set_projection_transform(disp, &t);

   ALLEGRO_BITMAP *tmp = al_create_bitmap(bitmap->lock_w, bitmap->lock_h);
   ALLEGRO_LOCKED_REGION *lr = al_lock_bitmap(tmp, lock_format, ALLEGRO_LOCK_WRITEONLY);
   int pixel_size = al_get_pixel_size(lock_format);
   int y;
   for (y = 0; y < bitmap->lock_h; y++) {
      uint8_t *p = ((uint8_t *)lr->data + lr->pitch * y);
      int pitch2 = ogl_pitch(bitmap->lock_w, pixel_size);
      uint8_t *p2 = ((uint8_t *)ogl_bitmap->lock_buffer + pitch2 * y);
      memcpy(p, p2, pixel_size*bitmap->lock_w);
   }
   al_unlock_bitmap(tmp);

   if (!already_is_target) {
      al_set_target_backbuffer(disp);
   }

   bool held = al_is_bitmap_drawing_held();
   al_hold_bitmap_drawing(false);
   al_draw_bitmap(tmp, bitmap->lock_x, bitmap->lock_y, ALLEGRO_FLIP_VERTICAL);
   al_hold_bitmap_drawing(held);
   al_destroy_bitmap(tmp);

   al_restore_state(&st);
   al_set_projection_transform(disp, &backup1);
   al_use_transform(&backup2);
}


#endif

/* vim: set sts=3 sw=3 et: */
