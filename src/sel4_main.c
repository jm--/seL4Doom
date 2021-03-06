/*
 * Copyright (c) 2015, Josef Mihalits
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "COPYING" for details.
 *
 */

#include <autoconf.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <cpio/cpio.h>
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
#include "sel4.local/libplatsupport/keyboard_ps2.h"
#include "sel4.local/libplatsupport/keyboard_chardev.h"

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

/* files linked in via archive.o */
extern char _cpio_archive[];

/* IRQHandler cap (with cspace path) */
static cspacepath_t kb_handler;

/* endpoint cap - waiting for IRQ */
static vka_object_t kb_ep;

/* input buffer for console input */
#define CMDLINE_LEN    1024
static char cmdline[CMDLINE_LEN];


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
init_timer()
{
    // get an endpoint for the timer IRQ (interrupt handler)
    UNUSED int err = vka_alloc_async_endpoint(&vka, &timer_aep);
    assert(err == 0);

    // get the timer
    timer = sel4platsupport_get_default_timer(&vka, &vspace, &simple, timer_aep.cptr);
    assert(timer != NULL);
}


// creates IRQHandler cap "handler" for IRQ "irq"
static void
get_irqhandler_cap(int irq, cspacepath_t* handler)
{
    seL4_CPtr cap;
    // get a cspace slot
    UNUSED int err = vka_cspace_alloc(&vka, &cap);
    assert(err == 0);

    // convert allocated cptr to a cspacepath, for use in
    // operations such as Untyped_Retype
    vka_cspace_make_path(&vka, cap, handler);

    // exec seL4_IRQControl_Get(seL4_CapIRQControl, irq, ...)
    // to get an IRQHandler cap for IRQ "irq"
    err = simple_get_IRQ_control(&simple, irq, *handler);
    assert(err == 0);
}


/*
 * Wait for (n * 10) ms.
 */
static void
isleep(int n) {
    for (int i = 0; i < n; i++) {
        UNUSED int err = timer_oneshot_relative(timer->timer, 10 * NS_IN_MS);
        assert(err == 0);
        seL4_Wait(timer_aep.cptr, NULL);
        sel4_timer_handle_single_irq(timer);
    }
}


static void
init_keyboard() {
    static const int keyboard_irqs[] = {KEYBOARD_PS2_IRQ, -1};
    static const struct dev_defn keyboard_def = {
            .id      = PC99_KEYBOARD_PS2,
            .paddr   = 0,
            .size    = 0,
            .irqs    = keyboard_irqs,
            .init_fn = &keyboard_cdev_init
    };
    int n = 0;
    int err = 0;
    do {
        //call into local platsuppport code
        err = keyboard_cdev_init(&keyboard_def, &io_ops, &inputdev);
        // The code to initialize the keyboard in platsupport
        // does not return PS2_CONTROLLER_SELF_TEST_OK when a key is pressed
        // before or during initialization or something?
        if (n++ == 100) {
            // We retry a couple of times before giving up. This gives the
            // user a chance to release a pressed key.
            printf("Failed to initialize PS2 keyboard.\n");
            exit(EXIT_FAILURE);
        }
    } while (err);

    //create IRQHandler cap
    get_irqhandler_cap(KEYBOARD_PS2_IRQ, &kb_handler);

    // create endpoint
    err = vka_alloc_async_endpoint(&vka, &kb_ep);
    assert(err == 0);

    /* Assign AEP to the IRQ handler. */
    err = seL4_IRQHandler_SetEndpoint(kb_handler.capPtr, kb_ep.cptr);
    assert(err == 0);

    /* Give keyboard time to settle down. Wait for finals ACKs generated
     * in keyboard_init() to show up. I need this for my (real) laptop.
     */
    isleep(100);
    /* Remove ACKs (or whatever) from keyboard buffer */
    keyboard_flush(&inputdev.ioops);
    err = seL4_IRQHandler_Ack(kb_handler.capPtr);
    assert(err == 0);
}


int
sel4doom_get_getchar() {
    return ps_cdev_getchar(&inputdev);
}


int
sel4doom_keyboard_poll_keyevent(int16_t* vkey) {
    return keyboard_poll_keyevent(vkey);
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


/*
 * Make VBE mode info available.
 */
void
sel4doom_get_vbe(seL4_VBEModeInfoBlock* mib) {
    *mib = bootinfo2->vbeModeInfoBlock;
}


static void
gfx_map_video_ram(ps_io_mapper_t *io_mapper) {
    seL4_VBEModeInfoBlock* mib = &bootinfo2->vbeModeInfoBlock;
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
    seL4_VBEModeInfoBlock* mib = &bootinfo2->vbeModeInfoBlock;
    const size_t size = mib->yRes * mib->linBytesPerScanLine;
    for (int i = 0; i < size / 4; i++) {
        /* set pixel;
         * depending on color depth, one pixel is 1, 2, or 3 bytes */
        p[i] = i; //generates some pattern
    }
}


void*
sel4doom_load_file(const char* filename) {
    UNUSED unsigned long filesize;
    return cpio_get_file(_cpio_archive, filename, &filesize);
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


/*
 * Print the files in the cpio archive.
 */
static void
list_files() {
    struct cpio_info info;
    cpio_info(_cpio_archive, &info);
    printf("The cpio archive contains %d file(s)\n", info.file_count);

    // length of one slot in "buffer" (+1 to include string terminating "\0")
    int slot_len = info.max_path_sz + 1;
    size_t size = slot_len * info.file_count;
    char* buffer = (char*) malloc(size);
    char** bufv = (char**) malloc(sizeof(char*) * info.file_count);
    assert(buffer && bufv);
    // fill with 0s as libcpio does not write "\0" at end of strings
    memset(buffer, 0, size);

    for (int i = 0; i < info.file_count; i++) {
        bufv[i] = buffer + slot_len * i;
    }
    cpio_ls(_cpio_archive, bufv, info.file_count);
    for (int i = 0; i < info.file_count; i++) {
        printf("file %d: [%s]\n", i + 1, bufv[i]);
    }
    free(buffer);
    free(bufv);
}


/*
 * Read one input string from the keyboard and store it in buf.
 */
static void
readline(char* buf, int buf_len) {
    int c; // current key char
    int pos = 0; // index in "buf" where c will be placed
    int err;

    for (;;) {
        fflush(stdout);
        seL4_Wait(kb_ep.cptr, NULL);
        err = seL4_IRQHandler_Ack(kb_handler.capPtr);
        assert(err == 0);
        for (;;) {
            c = sel4doom_get_getchar();
            if (c == -1) {
                // read till EOF
                break;
            }
            if (c == 13 && pos < buf_len) {
                // enter key
                buf[pos] = '\0';
                printf("\n");
                return; //DONE
            }
            if (c == 8) {
                // backspace key
                if (pos > 0) {
                    pos--;
                    printf("%c %c", c, c);
                }
            } else if (pos < buf_len - 1) {
                // add char to buffer; make sure there's space for final "\0"
                printf("%c", c);
                buf[pos++] = (char) c;
            }
        }
    }
}


/*
 * A command line console. It works nicely for me when running QEMU with
 *  "-serial stdio", but there is probably little real world use for it.
 *  It's main purpose is to provide a quick way to launch custom WAD files:
 *  e.g. with "doom -file myfile.wad". Also, every operating system needs
 *  a console. :)
 */
static void
run_console(int* argc, char* argv[]) {
    for (;;) {
        printf("seL4 > ");
        readline(cmdline, CMDLINE_LEN);
        /* "parse" command line (simply break at space char) */
        char* cmd = argv[0] = strtok(cmdline," ");
        for (*argc = 0; argv[*argc] != NULL;) {
            argv[++(*argc)] = strtok(NULL," ");
        }
        if (cmd == NULL) {
            // empty string; do nothing
        } else if (strcmp(cmd, "ls") == 0) {
            list_files();
        } else if (strcmp(cmd, "doom") == 0) {
            return; // DONE
        } else {
            printf("%s: command not found\n", cmd);
        }
    }
}


UNUSED static void
*main_continued()
{
    printf("initializing timer\n");
    init_timer();

    printf("initializing keyboard\n");
    init_keyboard();

    printf("initializing graphics\n");
    gfx_print_IA32BootInfo(bootinfo2);
    if (bootinfo2 == NULL
    || bootinfo2->vbeModeInfoBlock.xRes < 320
    || bootinfo2->vbeModeInfoBlock.yRes < 200
    || bootinfo2->vbeModeInfoBlock.bitsPerPixel != 32) {
        /* DOOM is 320x200, but any resolution should work */
        printf("Error: minimum graphics requirements not met\n");
        printf("Please boot the kernel in graphics mode ");
        printf("with a color depth of 32 bpp!\n\n");
        exit(EXIT_FAILURE);
    }
    gfx_map_video_ram(&io_ops.io_mapper);
    gfx_display_testpic();

    printf("initializing timers (you may see some errors or warnings)\n");
    fflush(stdout);
    // get a TSC timer (forward marching time); use
    // timer_get_time(tsc_timer) to get current time (in ns)
    tsc_timer = sel4platsupport_get_tsc_timer(timer);
    assert(tsc_timer != NULL);
    printf("done initializing timers\n");

    /* print content of cpio file */
    list_files();

    /* default command line arguments */
    int argc = 1;
    char* argv[CMDLINE_LEN / 2] = {"./doom", NULL};

    /* boot into console if 'c' was pressed */
    int c = sel4doom_get_getchar();
    if (c == 'c' || c == 'C') {
        run_console(&argc, argv);
    }

    /* we never return */
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
