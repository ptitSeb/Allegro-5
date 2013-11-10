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
 *      Configuration defines for use on Pandora platforms.
 *
 *      By ptitSeb.
 *
 *      See readme.txt for copyright information.
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

/* Describe this platform.  */
#define ALLEGRO_PLATFORM_STR  "PANDORA"

#define ALLEGRO_EXTRA_HEADER "allegro5/platform/alpandora.h"
#define ALLEGRO_INTERNAL_HEADER "allegro5/platform/aintpandora.h"
#define ALLEGRO_INTERNAL_THREAD_HEADER "allegro5/platform/aintuthr.h"

#define ALLEGRO_EXCLUDE_GLX

#ifndef ALLEGRO_PANDORA
#define ALLEGRO_PANDORA
#endif