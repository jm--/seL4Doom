/*
 * Copyright (c) 2015, Josef Mihalits
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 */

#include <assert.h>
#include <sel4/arch/bootinfo.h>
#include "SDL.h"

Uint32 sel4doom_get_current_time();
void * sel4doom_get_framebuffer_vaddr();
void sel4doom_get_vbe(seL4_VBEModeInfoBlock* mib);
int sel4doom_get_kb_state(int16_t* vkey, int16_t* extmode);

/* the current color palette in 32bit format as used by frame buffer */
uint32_t sel4doom_colors32[256];

static SDL_Surface sdl_surface;

/* VBE mode info */
static seL4_VBEModeInfoBlock mib;

/* base address of frame buffer (is virtual address) */
uint32_t* sel4doom_fb;


DECLSPEC SDL_Surface * SDLCALL
sel4doom_init_graphics(int* multiply) {
    int width = 320;
    int height = 200;
    *multiply = 1;

    sel4doom_get_vbe(&mib);

    //some default auto-detection for now
    if (mib.xRes == 320 * 2) { *multiply = 2; }
    if (mib.xRes == 320 * 3) { *multiply = 3; }
    width *= *multiply;
    height *= *multiply;
    printf("seL4: sel4doom_init_graphics: xRes=%d yRes=%d ==> w=%d h=%d multiply=%d\n",
            mib.xRes, mib.yRes, width, height, *multiply);

    sel4doom_fb = sel4doom_get_framebuffer_vaddr();
    assert(sel4doom_fb);

    sdl_surface = (SDL_Surface) {
        .flags = 0,
        .format = NULL,
        .w = width,
        .h = height,
        .pitch = width,
        .pixels = 0, //fb,
        .offset = 0,
        .hwdata = NULL,
        //.clip_rect = (SDL_Rect){.x = 0, .y = 0, .w = width, .h = height},
        .unused1 = 0,
        .locked = 0,
        .map = NULL,  // this is used
        .format_version = 3,
        .refcount = 1
    };
    return &sdl_surface;
}


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
    printf("seL4: SDL_Init(): flags=0x%x\n", flags);
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


DECLSPEC void SDLCALL 
SDL_Delay(Uint32 ms) {
    // printf("seL4: SDL_Delay(): ms=%d\n", ms);
}


DECLSPEC Uint8 SDLCALL 
SDL_GetMouseState(int *x, int *y) {
    return 0;
}


DECLSPEC int SDLCALL 
SDL_PollEvent(SDL_Event *event) {
    //printf("seL4: SDL_PollEvent()\n");

    int16_t vkey;
    int16_t extmode;
    int pressed = sel4doom_get_kb_state(&vkey, &extmode);
    if (vkey == -1) {
        //no pending events
        return 0;
    }

    event->type = pressed ? SDL_KEYDOWN : SDL_KEYUP;

    if (extmode) {
        //cursor keys
        switch(vkey) {
        case 0x25:
            event->key.keysym.sym = SDLK_LEFT;
            break;
        case 0x26:
            event->key.keysym.sym = SDLK_UP;
            break;
        case 0x27:
            event->key.keysym.sym = SDLK_RIGHT;
            break;
        case 0x28:
            event->key.keysym.sym = SDLK_DOWN;
            break;
        }
        return 1;
    }

    switch(vkey) {
    case 27:
        event->key.keysym.sym = SDLK_ESCAPE;
        break;
    case 13: //enter key
        event->key.keysym.sym = SDLK_RETURN;
        break;
    case 74: // 'j'
        event->key.keysym.sym = SDLK_LEFT;
        break;
    case 73: // 'i'
        event->key.keysym.sym = SDLK_UP;
        break;
    case 76: // 'l'
        event->key.keysym.sym = SDLK_RIGHT;
        break;
    case 75: // 'k'
        event->key.keysym.sym = SDLK_DOWN;
        break;
    case 32: // space
        event->key.keysym.sym = SDLK_SPACE;
        break;
    case 78: // 'n'
        event->key.keysym.sym = SDLK_n;
        break;
    case 89: // 'y'
        event->key.keysym.sym = SDLK_y;
        break;
    case 160:
        event->key.keysym.sym = SDLK_LSHIFT;
        break;
    case 161:
        event->key.keysym.sym = SDLK_RSHIFT;
        break;
    case 162:
        event->key.keysym.sym = SDLK_LCTRL;
        break;
//    case 0:
//        event->key.keysym.sym = SDLK_RCTRL;
//        break;
    case 18:
        event->key.keysym.sym = SDLK_LALT;
        break;
//    case 0:
//        event->key.keysym.sym = SDLK_RALT;
//        break;
    case 189:
        event->key.keysym.sym = SDLK_EQUALS;
        break;
    case 187:
        event->key.keysym.sym = SDLK_MINUS;
        break;
//    default:
//        event->key.keysym.sym = vkey;
//        break;
    }
    return 1;
}




DECLSPEC int SDLCALL 
SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors) {
    for (int i = firstcolor; i < ncolors; i++) {
        sel4doom_colors32[i] = (colors[i].r << mib.linRedOff)
                    | (colors[i].g << mib.linGreenOff)
                    | (colors[i].b << mib.linBlueOff);
    }
    return 1;
}




DECLSPEC void SDLCALL SDL_Quit(void) {
    printf("seL4: SDL_Quit()\n");
    //exit(0);
}
