// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for SDL library
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <assert.h>
#include <sel4/arch/bootinfo.h>
#include "SDL.h"

#include "m_swap.h"
#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"


void* sel4doom_get_framebuffer_vaddr();
void sel4doom_get_vbe(seL4_VBEModeInfoBlock* mib);
int sel4doom_get_kb_state(int16_t* vkey, int16_t* extmode);


/* the current color palette in 32bit format as used by frame buffer */
static uint32_t sel4doom_colors32[256];

/* VBE mode info */
static seL4_VBEModeInfoBlock mib;

/* base address of frame buffer (is virtual address) */
static uint32_t* sel4doom_fb;

/* width of real screen in pixels (i.e. 320 * multiply) */
static int pitch;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;



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


void I_ShutdownGraphics(void)
{
}


//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}


//
// I_StartTic
//
void I_StartTic(void)
{
    event_t event;
    while (sel4doom_poll_event(&event)) {
        D_PostEvent(&event);
    }
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}


//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    static int  lasttic;
    int     tics;
    int     i;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

        i = I_GetTime();
        tics = i - lasttic;
        lasttic = i;
        if (tics > 20) tics = 20;

        for (i=0 ; i<tics*2 ; i+=2)
            screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
        for ( ; i<20*2 ; i+=2)
            screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    }

    if (multiply == 1)
    {
        unsigned int *src = (unsigned int *) screens[0];
        unsigned int *dst = sel4doom_fb;
        static const int n = SCREENHEIGHT * SCREENWIDTH / 4 ;
        int i = n;
        while (i--)
        {
            /* We process four pixels per iteration. */
            unsigned int fourpix = *src++;

            //first "src" pixel
            *dst++ = sel4doom_colors32[fourpix & 0xff];

            //second "src" pixel
            *dst++ = sel4doom_colors32[(fourpix >> 8) & 0xff];

            //third "src" pixel
            *dst++ = sel4doom_colors32[(fourpix >> 16) & 0xff];

            //fourth "src" pixel
            *dst++ = sel4doom_colors32[fourpix >> 24];
        }
    }
    else if (multiply == 2)
    {
        /* pointer into source screen */
        unsigned int *src = (unsigned int *) (screens[0]);

        /*indices into frame buffer, one per row */
        int dst[2] = {0, pitch};

        int y = SCREENHEIGHT;
        while (y--)
        {
            int x = SCREENWIDTH;
            do
            {
                /* We process four "src" pixels per iteration
                 * and for every source pixel, we write out 4 pixels to "dst".
                 */
                unsigned fourpix = *src++;

                /* 32 bit RGB value */
                unsigned int p;
                //first "src" pixel
                p = sel4doom_colors32[fourpix & 0xff];
                sel4doom_fb[dst[0]++] = p;  //top left
                sel4doom_fb[dst[0]++] = p;  //bottom left
                sel4doom_fb[dst[1]++] = p;  //top right
                sel4doom_fb[dst[1]++] = p;  //bottom right

                //second "src" pixel
                p = sel4doom_colors32[(fourpix >> 8) & 0xff];
                sel4doom_fb[dst[0]++] = p;
                sel4doom_fb[dst[0]++] = p;
                sel4doom_fb[dst[1]++] = p;
                sel4doom_fb[dst[1]++] = p;

                //third "src" pixel
                p = sel4doom_colors32[(fourpix >> 16) & 0xff];
                sel4doom_fb[dst[0]++] = p;
                sel4doom_fb[dst[0]++] = p;
                sel4doom_fb[dst[1]++] = p;
                sel4doom_fb[dst[1]++] = p;

                //fourth "src" pixel
                p = sel4doom_colors32[fourpix >> 24];
                sel4doom_fb[dst[0]++] = p;
                sel4doom_fb[dst[0]++] = p;
                sel4doom_fb[dst[1]++] = p;
                sel4doom_fb[dst[1]++] = p;
            } while (x-=4);

            dst[0] += pitch;
            dst[1] += pitch;
        }

    }
    else if (multiply == 3)
    {
        unsigned int *olineptrs[3];
        unsigned int *ilineptr;
        int x, y, i;
        unsigned int fouropixels[3];
        unsigned int fouripixels;

        ilineptr = (unsigned int *) (screens[0]);
        for (i=0 ; i<3 ; i++) {
            olineptrs[i] =
                    (unsigned int *)&((Uint8 *)0)[i*pitch];
        }

        y = SCREENHEIGHT;
        while (y--)
        {
            x = SCREENWIDTH;
            do
            {
                fouripixels = *ilineptr++;
                fouropixels[0] = (fouripixels & 0xff000000)
                    |   ((fouripixels>>8) & 0xff0000)
                    |   ((fouripixels>>16) & 0xffff);
                fouropixels[1] = ((fouripixels<<8) & 0xff000000)
                    |   (fouripixels & 0xffff00)
                    |   ((fouripixels>>8) & 0xff);
                fouropixels[2] = ((fouripixels<<16) & 0xffff0000)
                    |   ((fouripixels<<8) & 0xff00)
                    |   (fouripixels & 0xff);
#ifdef __BIG_ENDIAN__
                *olineptrs[0]++ = fouropixels[0];
                *olineptrs[1]++ = fouropixels[0];
                *olineptrs[2]++ = fouropixels[0];
                *olineptrs[0]++ = fouropixels[1];
                *olineptrs[1]++ = fouropixels[1];
                *olineptrs[2]++ = fouropixels[1];
                *olineptrs[0]++ = fouropixels[2];
                *olineptrs[1]++ = fouropixels[2];
                *olineptrs[2]++ = fouropixels[2];
#else
                *olineptrs[0]++ = fouropixels[2];
                *olineptrs[1]++ = fouropixels[2];
                *olineptrs[2]++ = fouropixels[2];
                *olineptrs[0]++ = fouropixels[1];
                *olineptrs[1]++ = fouropixels[1];
                *olineptrs[2]++ = fouropixels[1];
                *olineptrs[0]++ = fouropixels[0];
                *olineptrs[1]++ = fouropixels[0];
                *olineptrs[2]++ = fouropixels[0];
#endif
            } while (x-=4);
            olineptrs[0] += 2*pitch/4;
            olineptrs[1] += 2*pitch/4;
            olineptrs[2] += 2*pitch/4;
        }

    }
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
    for (int i = 0; i < 256; i++, palette += 3) {
        sel4doom_colors32[i] =
                      (gammatable[usegamma][*palette] << mib.linRedOff)
                    | (gammatable[usegamma][*(palette + 1)] << mib.linGreenOff)
                    | (gammatable[usegamma][*(palette + 2)] << mib.linBlueOff);
    }
}


void I_InitGraphics(void) {
    int width = SCREENWIDTH;
    int height = SCREENHEIGHT;
    multiply = 1;

    sel4doom_get_vbe(&mib);

    //some default auto-detection for now
    if (mib.xRes == SCREENWIDTH * 2) { multiply = 2; }
    if (mib.xRes == SCREENWIDTH * 3) { multiply = 3; }
    width *= multiply;
    height *= multiply;
    pitch = width;
    printf("seL4: sel4doom_init_graphics: xRes=%d yRes=%d ==> w=%d h=%d multiply=%d\n",
            mib.xRes, mib.yRes, width, height, multiply);

    sel4doom_fb = sel4doom_get_framebuffer_vaddr();
    assert(sel4doom_fb);


    screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);
    if (screens[0] == NULL) {
        I_Error("Couldn't allocate screen memory");
    }
}
