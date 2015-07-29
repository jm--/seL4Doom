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
static SDL_PixelFormat sdl_format;
static SDL_Palette sdl_palette;
//static SDL_Color sdl_color;
static SDL_Color sdl_colors[256];

/* VBE mode info */
static seL4_VBEModeInfoBlock mib;

/* base address of frame buffer (is virtual address) */
uint32_t* sel4doom_fb;


DECLSPEC SDL_Surface * SDLCALL
sel4doom_init_graphics(int* multiply) {
    Uint32 flags = 0;
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

    sdl_palette.ncolors = 256;
    sdl_palette.colors = sdl_colors;  // not yet initialized

    sdl_format = (SDL_PixelFormat) {
        .palette = 0,
        .BitsPerPixel = 8,
        .BytesPerPixel = 1,
        .Rloss = 8,
        .Gloss = 8,
        .Bloss = 8,
        .Aloss = 8,
        .Rshift = 0,
        .Gshift = 0,
        .Bshift = 0,
        .Ashift = 0,
        .Rmask = 0,
        .Gmask = 0,
        .Bmask = 0,
        .Amask = 0,
        .colorkey = 0,
        .alpha = 255
    };

    sdl_surface = (SDL_Surface) {
        .flags = flags,
        .format = &sdl_format,
        .w = width,
        .h = height,
        .pitch = width,
        .pixels = 0, //fb,
        .offset = 0,
        .hwdata = NULL,
        .clip_rect = (SDL_Rect){.x = 0, .y = 0, .w = width, .h = height},
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
SDL_LockSurface(SDL_Surface *surface) {
    return 0;
}


DECLSPEC void SDLCALL 
SDL_UnlockSurface(SDL_Surface *surface) {
}


DECLSPEC void SDLCALL 
SDL_UpdateRect (SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h) {
    //printf("seL4: SDL_UpdateRect(): x=%d, y=%d, w=%d, h=%d\n", x, y, w, h);
    assert(x == 0);
    assert(y == 0);
    assert(w == 0);
    assert(h == 0);
}


DECLSPEC int SDLCALL 
SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors) {
    printf("seL4: SDL_SetColors(): firstcolor=%d, ncolors=%d\n",
            firstcolor, ncolors);

    assert(firstcolor == 0);
    assert(ncolors == 256);
    assert(surface == &sdl_surface);
    //assert(sdl_surface.format->palette == &sdl_palette);
    assert(sdl_palette.ncolors == 256);
    assert(sdl_palette.colors == sdl_colors);

    for (int i = firstcolor; i < ncolors; i++) {
        sdl_colors[i] = colors[i];
        sel4doom_colors32[i] = (colors[i].r << mib.linRedOff)
                    | (colors[i].g << mib.linGreenOff)
                    | (colors[i].b << mib.linBlueOff);
    }
    return 1;
}


DECLSPEC SDL_Surface * 
SDLCALL SDL_SetVideoMode (int width, int height, int bpp, Uint32 flags) {
    printf("seL4: SDL_SetVideoMode(): width=%d, height=%d, bpp=%d, flags=0x%x\n",
            width, height, bpp, flags);

    sel4doom_get_vbe(&mib);
    sel4doom_fb = sel4doom_get_framebuffer_vaddr();
    assert(sel4doom_fb);

    //sdl_color = (SDL_Color) { .r = 0, .g = 0, .b = 0, .unused = 0 };

    //sdl_palette.ncolors = 256;
    //sdl_palette.colors = &sdl_color;
    sdl_palette.ncolors = 256;
    sdl_palette.colors = sdl_colors;  // not yet initialized

    sdl_format = (SDL_PixelFormat) {
        .palette = &sdl_palette,
        .BitsPerPixel = 8,
        .BytesPerPixel = 1,
        .Rloss = 8,
        .Gloss = 8,
        .Bloss = 8,
        .Aloss = 8,
        .Rshift = 0,
        .Gshift = 0,
        .Bshift = 0,
        .Ashift = 0,
        .Rmask = 0,
        .Gmask = 0,
        .Bmask = 0,
        .Amask = 0,
        .colorkey = 0,
        .alpha = 255
    };

    sdl_surface = (SDL_Surface) {
        .flags = flags,
        .format = &sdl_format,
        .w = width,
        .h = height,
        .pitch = width,
        .pixels = sel4doom_fb,
        .offset = 0,
        .hwdata = NULL,
        .clip_rect = (SDL_Rect){.x = 0, .y = 0, .w = width, .h = height},
        .unused1 = 0,
        .locked = 0,
        .map = NULL,  // this is used
        .format_version = 3,
        .refcount = 1
    };

    return &sdl_surface;
}


DECLSPEC int SDLCALL 
SDL_ShowCursor(int toggle) {
    return 0;
}


DECLSPEC void SDLCALL 
SDL_WM_SetCaption(const char *title, const char *icon) {
}


DECLSPEC void SDLCALL SDL_Quit(void) {
    printf("seL4: SDL_Quit()\n");
    //exit(0);
}
