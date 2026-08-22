#pragma once

#if defined(__SDL3__)
#ifndef SDL_ENABLE_OLD_NAMES
#define SDL_ENABLE_OLD_NAMES
#endif
#include <SDL3/SDL.h>
#else
#include <SDL2/SDL.h>
#endif
