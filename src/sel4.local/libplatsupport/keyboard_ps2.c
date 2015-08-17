/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "keyboard_ps2.h"
#include "keyboard_vkey.h"
#include <stdlib.h>
#include <string.h>
#include <sel4/sel4.h>
#include <assert.h>

static void
ps2_single_control(ps_io_ops_t *ops, int8_t byte)
{
    ps_io_port_out(&ops->io_port_ops, PS2_IOPORT_CONTROL, 1, byte);
}

static void
ps2_dual_control(ps_io_ops_t *ops, int8_t byte1, int8_t byte2)
{
    ps_io_port_out(&ops->io_port_ops, PS2_IOPORT_CONTROL, 1, byte1);
    ps_io_port_out(&ops->io_port_ops, PS2_IOPORT_DATA, 1, byte2);
}

static uint8_t
ps2_read_control_status(ps_io_ops_t *ops)
{
    uint32_t res = 0;
    int error = ps_io_port_in(&ops->io_port_ops, PS2_IOPORT_CONTROL, 1, &res);
    assert(!error);
    (void) error;
    return (uint8_t) res;
}

static uint8_t
ps2_read_data(ps_io_ops_t *ops)
{
    uint32_t res = 0;
    int error = ps_io_port_in(&ops->io_port_ops, PS2_IOPORT_DATA, 1, &res);
    assert(!error);
    (void) error;
    return (uint8_t) res;
}

static void
ps2_write_output(ps_io_ops_t *ops, uint8_t byte)
{
    while ( (ps2_read_control_status(ops) & 0x2) != 0);
    ps_io_port_out(&ops->io_port_ops, PS2_IOPORT_DATA, 1, byte);
}

static uint8_t
ps2_read_output(ps_io_ops_t *ops)
{
    while ( (ps2_read_control_status(ops) & 0x1) == 0);
    return ps2_read_data(ops);
}

static void
ps2_send_keyboard_cmd(ps_io_ops_t *ops, uint8_t cmd)
{
    do {
        ps2_write_output(ops, cmd);
    } while (ps2_read_output(ops) != KEYBOARD_ACK);
}

static void
ps2_send_keyboard_cmd_param(ps_io_ops_t *ops, uint8_t cmd, uint8_t param)
{
    do {
        ps2_write_output(ops, cmd);
        ps2_write_output(ops, param);
    } while (ps2_read_output(ops) != KEYBOARD_ACK);
}

/* ---------------------------------------------------------------------------------------------- */
/*
 * Code for scanset 1 cannot handle "print screen" key or similar keys
 * that is, keys that start with 0xE0 but then have more than one code following
 */
static keyboard_key_event_t
keyboard_state_push_ps2_keyevent(struct keyboard_state *s, uint16_t ps2_keyevent)
{
    assert(s->scanset == 1 || s->scanset == 2);
    keyboard_key_event_t ev_none = { .vkey = -1, .pressed = false };

    if (s->state == KEYBOARD_PS2_STATE_IGNORE) {
        s->num_ignore--;
        if (s->num_ignore == 0) {
            s->state = KEYBOARD_PS2_STATE_NORMAL;
        }
        return ev_none;
    }

    assert(s->state & KEYBOARD_PS2_STATE_NORMAL);

    /* Handle release / extended mode keys. */
    switch (ps2_keyevent) {
    case KEYBOARD_PS2_EVENTCODE_RELEASE:
        if (s->scanset == 2) {
        s->state |= KEYBOARD_PS2_STATE_RELEASE_KEY;
        return ev_none;
        }
        break;
    case KEYBOARD_PS2_EVENTCODE_EXTENDED:
        s->state |= KEYBOARD_PS2_STATE_EXTENDED_MODE;
        return ev_none;
    case KEYBOARD_PS2_EVENTCODE_EXTENDED_PAUSE:
        s->state = KEYBOARD_PS2_STATE_IGNORE;
        if (s->scanset == 2) {
        s->num_ignore = 7; /* Ignore the next 7 characters of pause seq. */
        } else {
            s->num_ignore = 5; /* Ignore the next 5 characters of pause seq. */
        }
        keyboard_key_event_t ev = { .vkey = VK_PAUSE, .pressed = true };
        return ev;
    }

    /* Prepend 0xE0 to ps2 keycode if in extended mode. */
    if (s->state & KEYBOARD_PS2_STATE_EXTENDED_MODE) {
        ps2_keyevent = 0xE000 + (ps2_keyevent & 0xFF);
        s->state &= ~KEYBOARD_PS2_STATE_EXTENDED_MODE;
    }

    int16_t vkey;
    int pressed;
    if (s->scanset == 1) {
        pressed = (0 == (ps2_keyevent & 0x80));
        ps2_keyevent &= (~0x80);
        vkey = keycode_ps2_to_vkey_set1(ps2_keyevent);
    } else {
        pressed = true;
        vkey = keycode_ps2_to_vkey_set2(ps2_keyevent);
    }

    if (vkey < 0) {
        /* No associated vkey with this PS2 key. */
        s->state = KEYBOARD_PS2_STATE_NORMAL;
        return ev_none;
    }

    if (s->scanset == 2) {
    /* Set keystate according to press or release. */
    if (s->state & KEYBOARD_PS2_STATE_RELEASE_KEY) {
        /* Release event. */
            pressed = false;
        s->state &= ~KEYBOARD_PS2_STATE_RELEASE_KEY;
        }
    }

    keyboard_key_event_t ev = { .vkey = vkey, .pressed = pressed };
    return ev;
}


/* ---------------------------------------------------------------------------------------------- */

int
keyboard_init(struct keyboard_state *state, const ps_io_ops_t* ops,
              void (*handle_event_callback)(keyboard_key_event_t ev, void *cookie))
{
    assert(state && ops);
    memset(state, 0, sizeof(struct keyboard_state));

    state->state = KEYBOARD_PS2_STATE_NORMAL;
    state->ops = *ops;
    state->scanset = 2;
    state->handle_event_callback = handle_event_callback;

    /* Initialise the PS2 keyboard device. */

    /* Disable both PS2 devices. */
    ps2_single_control(&state->ops, PS2_CMD_DISABLE_KEYBOARD_INTERFACE);
    ps2_single_control(&state->ops, PS2_CMD_DISABLE_MOUSE_INTERFACE);

    /* Flush the output buffer. */
    ps2_read_data(&state->ops);

    /* Enable IRQs and disable translation (IRQ bits 0, 1, translation 6). */
    ps2_single_control(&state->ops, PS2_READ_CMD_BYTE);
    uint8_t config = ps2_read_output(&state->ops);
    config |= 0x1;
    config &= 0xBF;
    ps2_dual_control(&state->ops, PS2_WRITE_CMD_BYTE, config);

    /* Run a controller self test. */
    ps2_single_control(&state->ops, PS2_CMD_CONTROLLER_SELF_TEST);
    uint8_t res = ps2_read_output(&state->ops);
    if (res != PS2_CONTROLLER_SELF_TEST_OK) {
        return -1;
    }

    /* Run keyboard interface test. */
    ps2_single_control(&state->ops, PS2_CMD_KEYBOARD_INTERFACE_TEST);
    res = ps2_read_output(&state->ops);
    if (res != 0) {
        return -1;
    }

    /* Enable keyboard interface. */
    ps2_single_control(&state->ops, PS2_CMD_ENABLE_KEYBOARD_INTERFACE);
    ps2_read_data(&state->ops);

    /* Reset the keyboard device. */
    if (keyboard_reset(state)) {
        return -1;
    }

    /* Set scanmode 2. */
    keyboard_set_scanmode(state, 2);
    return 0;
}

void
keyboard_set_led(struct keyboard_state *state, char scroll_lock, char num_lock, char caps_lock)
{
    ps2_send_keyboard_cmd_param(&state->ops, KEYBOARD_SET_LEDS,
                                scroll_lock | num_lock << 1 | caps_lock << 2);
}

void
keyboard_set_scanmode(struct keyboard_state *state, uint8_t mode)
{
    ps2_send_keyboard_cmd(&state->ops, KEYBOARD_DISABLE_SCAN); /* Disable scanning. */
    ps2_send_keyboard_cmd_param(&state->ops, KEYBOARD_SET_SCANCODE_MODE, mode); /* Set scan code. */
    ps2_send_keyboard_cmd(&state->ops, KEYBOARD_ENABLE_SCAN); /* Re-Enable scanning. */
}

int
keyboard_reset(struct keyboard_state *state)
{
    /* Reset the keyboard device. */
    ps2_send_keyboard_cmd(&state->ops, KEYBOARD_RESET);

    /* Wait for the Basic Assurance Test. */
    while (1) {
        uint8_t res = ps2_read_output(&state->ops);
        if (res == KEYBOARD_BAT_SUCCESSFUL) {
            break;
        }
        if (res == KEYBOARD_ERROR) {
            assert(!"keyboard init keyboard BAT failed.");
            return -1;
        }
    }

    return 0;
}

keyboard_key_event_t
keyboard_poll_ps2_keyevent(struct keyboard_state *state)
{
    if ((ps2_read_control_status(&state->ops) & 0x1) == 0) {
        /* No key events generated. */
        keyboard_key_event_t ev = { .vkey = -1, .pressed = false };
        return ev;
    }
    return keyboard_state_push_ps2_keyevent(state, ps2_read_data(&state->ops));
}

void
keyboard_poll_ps2_keyevents(struct keyboard_state *state, void *cookie)
{
    keyboard_key_event_t ev = { .vkey = -1, .pressed = false };
    do {
        ev = keyboard_poll_ps2_keyevent(state);
        if (ev.vkey != -1 && state->handle_event_callback) {
            state->handle_event_callback(ev, cookie);
        }
    } while (ev.vkey != -1);
}

/*
 * Consider race condition: e.g. (1) you submit a keyboard command, (2) you
 * call keyboard_flush(); (3) ACK from step 1 gets written to buffer; so the
 * result is you called flush but buffer is not empty
 */
void
keyboard_flush(ps_io_ops_t *ops)
{
     for (;;) {
#ifdef KEYBOARD_KEY_DEBUG
        printf("keyboard_flush() control=%x, \n", ps2_read_control_status(ops));
#endif
        if (0 == (ps2_read_control_status(ops) & 0x1)) {
            break;
        }
        UNUSED uint8_t c = ps2_read_data(ops);
#ifdef KEYBOARD_KEY_DEBUG
        printf("keyboard_flush %x\n", c);
#endif
    }
}

int
keyboard_detect_scanset(ps_io_ops_t *ops)
{
    uint8_t c1;
    /* skip over some "special bytes" */
    do {
        c1 = ps2_read_output(ops);
#ifdef KEYBOARD_KEY_DEBUG
        printf("keyboard_detect_scanset() flush: %x\n", c1);
#endif
    } while (c1 >= KEYBOARD_ACK
          || c1 == 0x00
          || c1 == KEYBOARD_BAT_SUCCESSFUL);

    uint8_t c2 = ps2_read_output(ops);
#ifdef KEYBOARD_KEY_DEBUG
    printf("keyboard_detect_scanset() c1=%x c2=%x (0x80 + c1)=%x\n"
            , c1, c2, 0x80 + c1);
#endif
    if (0x80 + c1 == c2) {
        return 1;
    }
    if (c2 == KEYBOARD_PS2_EVENTCODE_RELEASE) {
        uint8_t c3 = ps2_read_output(ops);
        if (c1 == c3) {
            return 2;
        }
    }
    return -1;
}
