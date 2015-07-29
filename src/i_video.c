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

#include "SDL.h"

#include "m_swap.h"
#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

DECLSPEC SDL_Surface * SDLCALL sel4doom_init_graphics(int* multiply);
int sel4doom_poll_event(event_t* event);
extern unsigned int sel4doom_colors32[256];
extern unsigned int* sel4doom_fb;

SDL_Surface *screen;

// Fake mouse handling.
boolean		grabMouse;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;


void I_ShutdownGraphics(void)
{
  SDL_Quit();
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
        int dst[2] = {0, screen->pitch};

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

            dst[0] += screen->pitch;
            dst[1] += screen->pitch;
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
                    (unsigned int *)&((Uint8 *)screen->pixels)[i*screen->pitch];
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
            olineptrs[0] += 2*screen->pitch/4;
            olineptrs[1] += 2*screen->pitch/4;
            olineptrs[2] += 2*screen->pitch/4;
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
    int i;
    SDL_Color colors[256];

    for ( i=0; i<256; ++i ) {
	colors[i].r = gammatable[usegamma][*palette++];
	colors[i].g = gammatable[usegamma][*palette++];
	colors[i].b = gammatable[usegamma][*palette++];
	colors[i].unused = 0;
    }
    SDL_SetColors(screen, colors, 0, 256);
}



void I_InitGraphics(void) {
    static int  firsttime=1;
    if (!firsttime) {
        I_Error("I_InitGraphics(): not first time");
    }

    screen = sel4doom_init_graphics(&multiply);
    screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);
    if (screens[0] == NULL) {
        I_Error("Couldn't allocate screen memory");
    }

}
