SET(ALLEGRO_PANDORA 1)
SET(CMAKE_SYSTEM_NAME Linux)

SET(CMAKE_C_FLAGS "-mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -ffast-math -mno-unaligned-access -fdiagnostics-color=auto")
SET(CMAKE_CXX_FLAGS "-mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -ffast-math -mno-unaligned-access -fdiagnostics-color=auto")
SET(CMAKE_INSTALL_PREFIX "/mnt/utmp/codeblocks/usr")

set(CMAKE_LINKER ${TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}ld)
set(CMAKE_NM ${TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}nm)
set(CMAKE_OBJCOPY ${TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_STRIP ${TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}strip)
set(CMAKE_RANLIB ${TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}ranlib)

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(ALLEGRO_CFG_OPENGLES 1)

SET(ALLEGRO_EXCLUDE_GLX 1)

if(WANT_GLES2)
	SET(ALLEGRO_CFG_OPENGLES2 1)
	SET(ALLEGRO_CFG_OPENGL_PROGRAMMABLE_PIPELINE 1)
	set(OPENGL_LIBRARIES "/usr/lib/libGLESv2.so;/usr/lib/libEGL.so;/lib/librt.so.1;/mnt/utmp/codeblocks/usr/lib/libXfixes.so")
	set(OPENGL_gl_LIBRARY "/usr/lib/libGLESv2.so;/usr/lib/libEGL.so;/lib/librt.so.1;/mnt/utmp/codeblocks/usr/lib/libXfixes.so")
else(WANT_GLES2)
	set(OPENGL_LIBRARIES "/usr/lib/libGLES_CM.so;/usr/lib/libEGL.so;/lib/librt.so.1;/mnt/utmp/codeblocks/usr/lib/libXfixes.so")
	set(OPENGL_gl_LIBRARY "/usr/lib/libGLES_CM.so;/usr/lib/libEGL.so;/lib/librt.so.1;/mnt/utmp/codeblocks/usr/lib/libXfixes.so")
endif(WANT_GLES2)
set(OPENGL_glu_LIBRARY "")

include_directories(
   "/mnt/utmp/codeblocks/usr/include"
)

