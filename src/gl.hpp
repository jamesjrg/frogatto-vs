#ifndef GL_HPP_INCLUDED
#define GL_HPP_INCLUDED

#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA)
#include <GLES/gl.h>
#else
#define NO_SDL_GLEXT
#include "SDL_opengl.h"
#endif

#endif GL_HPP_INCLUDED