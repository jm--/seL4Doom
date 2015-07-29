/*
 * Copyright (c) 2015, Josef Mihalits
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 */


/*
 * All graphics related hacks for seL4 have been moved to i_video.c
 * This file now mainly contains empty stubs to make the program compile.
 */

#include "SDL.h"


Uint32 sel4doom_get_current_time();


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


//called like so: SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO);
DECLSPEC int SDLCALL 
SDL_Init(Uint32 flags) {
    return 0;
}


DECLSPEC char * SDLCALL 
SDL_GetError(void) {
    return "some SDL error occurred :(";
}


/*
 * http://wiki.libsdl.org/SDL_GetTicks
 * Use this function to get the number of milliseconds since the
 * SDL library initialization.
 */
DECLSPEC Uint32 SDLCALL 
SDL_GetTicks(void) {
    return sel4doom_get_current_time();
}


/*
 * Not implemented.
 * Function is only used to introduce a delay when quitting the game.
 */
DECLSPEC void SDLCALL 
SDL_Delay(Uint32 ms) {
}
