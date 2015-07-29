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
#include "doomdef.h"
#include "d_event.h"

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


int
sel4doom_poll_event(event_t* event) {
    int16_t vkey;
    int16_t extmode;
    int pressed = sel4doom_get_kb_state(&vkey, &extmode);
    if (vkey == -1) {
        //no pending events
        return 0;
    }

    event->type = pressed ? ev_keydown : ev_keyup;

    if (extmode) {
        //cursor keys
        switch(vkey) {
        case 0x25:
            event->data1 = KEY_LEFTARROW;
            return 1;
        case 0x26:
            event->data1 = KEY_UPARROW;
            return 1;
        case 0x27:
            event->data1 = KEY_RIGHTARROW;
            return 1;
        case 0x28:
            event->data1 = KEY_DOWNARROW;
            return 1;
        case 0x18: //right alt
            event->data1 = KEY_RALT;
            return 1;
        default:
            //this default mapping may not be correct?
            event->data1 = vkey;
            return 1;
        }
        assert(!"we shouldn't be here");
    }

    switch(vkey) {
    case 8:
        event->data1 = KEY_BACKSPACE;
        return 1;
    case 160: //left shift
    case 161: //right shift
        event->data1 = KEY_RSHIFT;
        return 1;
    case 162: //left control
        event->data1 = KEY_RCTRL;
        return 1;
    case 18: // left alt
        event->data1 = KEY_RALT;
        return 1;
    case 112:
        event->data1 = KEY_F1;
        return 1;
    case 113:
        event->data1 = KEY_F2;
        return 1;
    case 114:
        event->data1 = KEY_F3;
        return 1;
    case 115:
        event->data1 = KEY_F4;
        return 1;
    case 116:
        event->data1 = KEY_F5;
        return 1;
    case 117:
        event->data1 = KEY_F6;
        return 1;
    case 118:
        event->data1 = KEY_F7;
        return 1;
    case 119:
        event->data1 = KEY_F8;
        return 1;
    case 120:
        event->data1 = KEY_F9;
        return 1;
    case 121:
        event->data1 = KEY_F10;
        return 1;
    case 122:
        event->data1 = KEY_F11;
        return 1;
    case 123:
        event->data1 = KEY_F12;
        return 1;
    case 187:
        event->data1 = KEY_EQUALS;
        return 1;
    case 189:
        event->data1 = KEY_MINUS;
        return 1;
    case 19:
        event->data1 = KEY_PAUSE;
        return 1;
    }

    if (65 <= vkey && vkey <= 90) {
        //ASCII shift: upper case chars to lower case chars
        //IDKFA :->
        event->data1 = vkey + 32;
        return 1;
    }

    //covers ESC, Enter, digits 1-0
    event->data1 = vkey;
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
