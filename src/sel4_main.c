/*
 * Copyright (c) 2015, Josef Mihalits
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 */

#include <autoconf.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sel4/arch/bootinfo.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>
#include <platsupport/timer.h>
#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/arch/io.h>
#include <sel4utils/vspace.h>
#include <sel4utils/stack.h>
#include <simple-stable/simple-stable.h>

int main_ORIGINAL(int argc, char** argv);

/* memory management: Virtual Kernel Allocator (VKA) interface and VSpace */
static vka_t vka;
static vspace_t vspace;

/*system abstraction */
static simple_t simple;

/* root task's BootInfo */
static seL4_BootInfo *bootinfo;

/* root task's IA32_BootInfo */
static seL4_IA32_BootInfo *bootinfo2;

/* platsupport I/O */
static ps_io_ops_t io_ops;

/* amount of virtual memory for the allocator to use */
#define VIRT_POOL_SIZE (BIT(seL4_PageBits) * 200)

/* static memory for the allocator to bootstrap with */
#define POOL_SIZE (BIT(seL4_PageBits) * 10)
static char memPool[POOL_SIZE];

/* for virtual memory bootstrapping */
static sel4utils_alloc_data_t allocData;

/* platsupport (periodic) timer */
static seL4_timer_t* timer;

/* platsupport TSC based timer */
static seL4_timer_t* tsc_timer;

/* async endpoint for periodic timer */
static vka_object_t timer_aep;

/* input character device (e.g. keyboard, COM1) */
static ps_chardevice_t inputdev;

/* pointer to base address of (linear) frame buffer */
typedef void* fb_t;
static fb_t fb = NULL;

/* convenience pointer to VBE mode info */
static seL4_VBEModeInfoBlock* mib;


//////////////////////////////////////////////////////
/*  Types and prototype for keyboard access here are from
 *  keyboard_chardev.c and keyboard_ps2.h in the platsupport library.
 *  I guess one is not supposed to use them directly...
 */

#define KEYBOARD_PS2_STATE_NORMAL 0x1
//#define KEYBOARD_PS2_STATE_IGNORE 0x2
#define KEYBOARD_PS2_STATE_EXTENDED_MODE 0x4
#define KEYBOARD_PS2_STATE_RELEASE_KEY 0x8
//#define KEYBOARD_PS2_EVENTCODE_RELEASE 0xF0
//#define KEYBOARD_PS2_EVENTCODE_EXTENDED 0xE0
//#define KEYBOARD_PS2_EVENTCODE_EXTENDED_PAUSE 0xE1

typedef struct keyboard_key_event {
    int16_t vkey;
    bool pressed;
} keyboard_key_event_t;

struct keyboard_state {
    ps_io_ops_t ops;
    int state;
    int num_ignore;
    void (*handle_event_callback)(keyboard_key_event_t ev, void *cookie);
};

/* keyboard state */
static struct keyboard_state kb_state;

/* prototype of platsupport internal function */
keyboard_key_event_t keyboard_poll_ps2_keyevent(struct keyboard_state *state);

//////////////////////////////////////////////////////


// ======================================================================
/*
 * Initialize all main data structures.
 *
 * The code to initialize simple, allocman, vka, and vspace is modeled
 * after the "sel4test-driver" app:
 * https://github.com/seL4/sel4test/blob/master/apps/sel4test-driver/src/main.c
 */
static void
setup_system()
{
    /* initialize boot information */
    bootinfo  = seL4_GetBootInfo();
    bootinfo2 = seL4_IA32_GetBootInfo();
    assert(bootinfo2); // boot kernel in graphics mode

    /* initialize simple interface */
    simple_stable_init_bootinfo(&simple, bootinfo);
    //simple_default_init_bootinfo(simple, bootinfo);

    /* create an allocator */
    allocman_t *allocman;
    allocman = bootstrap_use_current_simple(&simple, POOL_SIZE, memPool);
    assert(allocman);

    /* create a VKA */
    allocman_make_vka(&vka, allocman);

    /* create a vspace */
    UNUSED int err;
    err = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&vspace,
            &allocData, seL4_CapInitThreadPD, &vka, bootinfo);
    assert(err == 0);

    /* fill allocator with virtual memory */
    void *vaddr;
    UNUSED reservation_t vres;
    vres = vspace_reserve_range(&vspace, VIRT_POOL_SIZE, seL4_AllRights,
            1, &vaddr);
    assert(vres.res);
    bootstrap_configure_virtual_pool(allocman, vaddr, VIRT_POOL_SIZE,
            seL4_CapInitThreadPD);

    /* initialize platsupport IO: virt memory */
    err = sel4platsupport_new_io_mapper(simple, vspace, vka, &io_ops.io_mapper);
    assert(err == 0);

    /* initialize platsupport IO: ports */
    err = sel4platsupport_get_io_port_ops(&io_ops.io_port_ops, &simple);
    assert(err == 0);
}


static void
init_timers()
{
    // get an endpoint for the timer IRQ (interrupt handler)
    UNUSED int err = vka_alloc_async_endpoint(&vka, &timer_aep);
    assert(err == 0);

    // get the timer
    timer = sel4platsupport_get_default_timer(&vka, &vspace, &simple, timer_aep.cptr);
    assert(timer != NULL);

    // get a TSC timer (forward marching time); use
    // timer_get_time(tsc_timer) to get current time (in ns)
    tsc_timer = sel4platsupport_get_tsc_timer(timer);
    assert(tsc_timer != NULL);
}


static void
init_keyboard() {
    ps_chardevice_t *ret;
    ret = ps_cdev_init(PC99_KEYBOARD_PS2, &io_ops, &inputdev);
    assert(ret != NULL);

    //initialize keyboard state;
    //mirroring platsupport's internal keyboard state; yikes
    kb_state.state = KEYBOARD_PS2_STATE_NORMAL;
    kb_state.ops = io_ops;   // keyboard.dev.ioops
    kb_state.num_ignore = 0;
    kb_state.handle_event_callback = NULL;
}


int
sel4doom_get_getchar() {
    return ps_cdev_getchar(&inputdev);
}


/* At the time of writing (July 2015), there seems to be a bug in
 * keyboard_state_push_ps2_keyevent(), which results in extmode not working.
 * Parameter ps2_keyevent should be uint16_t and not int16_t, I think.
 * (I'll review again and file an issue on github if necessary.)
 *
 * I still have to read-up on keyboard scan codes and related things...
 * Hacked this together to make it work for now.
 */
int
sel4doom_get_kb_state(int16_t* vkey, int16_t* extmode) {
    keyboard_key_event_t ev = keyboard_poll_ps2_keyevent(&kb_state);
    *extmode = kb_state.state & KEYBOARD_PS2_STATE_EXTENDED_MODE;

    if (*extmode) {
        assert(ev.vkey == -1 && ev.pressed  == 0);
        ev= keyboard_poll_ps2_keyevent(&kb_state);
    }

    if (*extmode &&  (kb_state.state & KEYBOARD_PS2_STATE_RELEASE_KEY)) {
        ev= keyboard_poll_ps2_keyevent(&kb_state);
    }

    if (ev.vkey != -1) {
        printf("key %s: extmode=%d vkey=%d\n",
                ev.pressed ? "DOWN":"UP  ", (*extmode > 0), ev.vkey);
    }
    *vkey = ev.vkey;
    return ev.pressed;
}


/*
 *  @return: time since start (in ms)
 */
uint32_t
sel4doom_get_current_time() {
    return timer_get_time(tsc_timer->timer) / NS_IN_MS;
}


/*
 * @return: base address of frame buffer
 */
void *
sel4doom_get_framebuffer_vaddr() {
    return fb;
}


static void
gfx_init_IA32BootInfo(seL4_IA32_BootInfo* bootinfo) {
    mib = &bootinfo->vbeModeInfoBlock;
}


static void
gfx_map_video_ram(ps_io_mapper_t *io_mapper) {
    size_t size = mib->yRes * mib->linBytesPerScanLine;
    fb = (fb_t) ps_io_map(io_mapper,
            mib->physBasePtr,
            size,
            0,
            PS_MEM_NORMAL);
    assert(fb != NULL);
}


static void
gfx_display_testpic() {
    assert(fb != NULL);
    uint32_t* p = (uint32_t*) fb;
    const size_t size = mib->yRes * mib->linBytesPerScanLine;
    for (int i = 0; i < size / 4; i++) {
        /* set pixel;
         * depending on color depth, one pixel is 1, 2, or 3 bytes */
        p[i] = i; //generates some pattern
    }
}



static void
gfx_print_IA32BootInfo(seL4_IA32_BootInfo* bootinfo) {
    seL4_VBEInfoBlock* ib      = &bootinfo->vbeInfoBlock;
    seL4_VBEModeInfoBlock* mib = &bootinfo->vbeModeInfoBlock;

    printf("\n");
    printf("--VBE information:\n");
    printf("vbeMode: 0x%x\n", bootinfo->vbeMode);
    printf("  VESA mode=%d; linear frame buffer=%d\n",
            (bootinfo->vbeMode & BIT(8)) != 0,
            (bootinfo->vbeMode & BIT(14)) != 0);
    printf("vbeInterfaceSeg: 0x%x\n", bootinfo->vbeInterfaceSeg);
    printf("vbeInterfaceOff: 0x%x\n", bootinfo->vbeInterfaceOff);
    printf("vbeInterfaceLen: 0x%x\n", bootinfo->vbeInterfaceLen);

    printf("ib->signature: %c%c%c%c\n",
            ib->signature[0],
            ib->signature[1],
            ib->signature[2],
            ib->signature[3]);
    printf("ib->version: %x\n", ib->version); //BCD

    printf("--seL4_VBEModeInfoBlock:\n");
    printf("modeAttr: 0x%x\n", mib->modeAttr);
    printf("winAAttr: 0x%x\n", mib->winAAttr);
    printf("winBAttr: 0x%x\n", mib->winBAttr);
    printf("winGranularity: %d\n", mib->winGranularity);
    printf("winSize: %d\n", mib->winSize);
    printf("winASeg: 0x%x\n", mib->winASeg);
    printf("winBSeg: 0x%x\n", mib->winBSeg);
    printf("winFuncPtr: 0x%x\n", mib->winFuncPtr);
    printf("bytesPerScanLine: %d\n", mib->bytesPerScanLine);
    /* VBE 1.2+ */
    printf("xRes: %d\n", mib->xRes);
    printf("yRes: %d\n", mib->yRes);
    printf("xCharSize: %d\n", mib->xCharSize);
    printf("yCharSize: %d\n", mib->yCharSize);
    printf("planes: %d\n", mib->planes);
    printf("bitsPerPixel: %d\n", mib->bitsPerPixel);
    printf("banks: %d\n", mib->banks);
    printf("memoryModel: 0x%x\n", mib->memoryModel);
    printf("bankSize: %d\n", mib->bankSize);
    printf("imagePages: %d\n", mib->imagePages);
    printf("reserved1: 0x%x\n", mib->reserved1);

    printf("redLen: %d\n", mib->redLen);
    printf("redOff: %d\n", mib->redOff);
    printf("greenLen: %d\n", mib->greenLen);
    printf("greenOff: %d\n", mib->greenOff);
    printf("blueLen: %d\n", mib->blueLen);
    printf("blueOff: %d\n", mib->blueOff);
    printf("rsvdLen: %d\n", mib->rsvdLen);
    printf("rsvdOff: %d\n", mib->rsvdOff);
    printf("directColorInfo: %d\n", mib->directColorInfo);

    /* VBE 2.0+ */
    printf("physBasePtr: %x\n", mib->physBasePtr);
    //printf("reserved2: %d\n", mib->reserved2[6]);

    /* VBE 3.0+ */
    printf("linBytesPerScanLine: %d\n", mib->linBytesPerScanLine);
    printf("bnkImagePages: %d\n", mib->bnkImagePages);
    printf("linImagePages: %d\n", mib->linImagePages);
    printf("linRedLen: %d\n", mib->linRedLen);
    printf("linRedOff: %d\n", mib->linRedOff);
    printf("linGreenLen: %d\n", mib->linGreenLen);
    printf("linGreenOff: %d\n", mib->linGreenOff);
    printf("linBlueLen: %d\n", mib->linBlueLen);
    printf("linBlueOff: %d\n", mib->linBlueOff);
    printf("linRsvdLen: %d\n", mib->linRsvdLen);
    printf("linRsvdOff: %d\n", mib->linRsvdOff);
    printf("maxPixelClock: %d\n", mib->maxPixelClock);
    printf("modeId: %d\n", mib->modeId);
    printf("depth: %d\n\n", mib->depth);
}


static void
*main_continued()
{
    printf("initializing keyboard\n");
    init_keyboard();

    gfx_print_IA32BootInfo(bootinfo2);
    gfx_init_IA32BootInfo(bootinfo2);
    gfx_map_video_ram(&io_ops.io_mapper);
    gfx_display_testpic();

    printf("initializing timers\n");
    fflush(stdout);
    init_timers();
    printf("done initializing timers\n");

    int argc = 1;
    char* argv[] = {
            "doom",
            NULL
    };

    /* we never return (I think) */
    main_ORIGINAL(argc, argv);
    return NULL;
}

int main()
{
    setup_system();

    /* enable serial driver */
    platsupport_serial_setup_simple(NULL, &simple, &vka);

    printf("\n\n========= Welcome to seL4Doom ========= \n\n");

    // stack size is configurable via CONFIG_SEL4UTILS_STACK_SIZE
    int err = (int)sel4utils_run_on_stack(&vspace, main_continued, NULL);
    assert(err == 0);
    printf("Bye!\n\n");
    return 0;
}
