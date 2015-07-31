/*
 * Copyright (c) 2015, Josef Mihalits
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "COPYING" for details.
 *
 */

#ifndef SEL4_DOOM_H_
#define SEL4_DOOM_H_

#include <sel4/arch/bootinfo.h>


void*
sel4doom_get_framebuffer_vaddr();


void
sel4doom_get_vbe(seL4_VBEModeInfoBlock* mib);


int
sel4doom_get_kb_state(int16_t* vkey, int16_t* extmode);


unsigned int
sel4doom_get_current_time();


#endif /* SEL4_DOOM_H_ */
