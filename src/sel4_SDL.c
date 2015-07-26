/*
 * Copyright (c) 2015, Josef Mihalits
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 */

#include "SDL.h"

DECLSPEC void SDLCALL 
SDL_LockAudio(void) {
}


DECLSPEC void SDLCALL 
SDL_UnlockAudio(void) {
}


DECLSPEC int SDLCALL 
SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
    return 0;
}


DECLSPEC void SDLCALL 
SDL_PauseAudio(int pause_on) {
}


DECLSPEC void SDLCALL 
SDL_CloseAudio(void) {
}


DECLSPEC int SDLCALL 
SDL_Init(Uint32 flags) {
    return 0;
}


DECLSPEC char * SDLCALL 
SDL_GetError(void) {
    return "some SDL error occurred :(";
}


DECLSPEC Uint32 SDLCALL 
SDL_GetTicks(void) {
}


DECLSPEC void SDLCALL 
SDL_Delay(Uint32 ms) {
}


DECLSPEC Uint8 SDLCALL 
SDL_GetMouseState(int *x, int *y) {
}


DECLSPEC int SDLCALL 
SDL_PollEvent(SDL_Event *event) {
}


DECLSPEC int SDLCALL 
SDL_LockSurface(SDL_Surface *surface) {
}


DECLSPEC void SDLCALL 
SDL_UnlockSurface(SDL_Surface *surface) {
}


DECLSPEC void SDLCALL 
SDL_UpdateRect (SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h) {
}


DECLSPEC int SDLCALL 
SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors) {
}


DECLSPEC SDL_Surface * 
SDLCALL SDL_SetVideoMode (int width, int height, int bpp, Uint32 flags) {
}


DECLSPEC int SDLCALL 
SDL_ShowCursor(int toggle) {
}


DECLSPEC void SDLCALL 
SDL_WM_SetCaption(const char *title, const char *icon) {
}


DECLSPEC void SDLCALL SDL_Quit(void) {
}
