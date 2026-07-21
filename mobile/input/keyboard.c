#include "keyboard.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>

/* Linux keycode to UOS keycode mapping (simplified) */
static int linux_to_uos_keycode(int linux_keycode)
{
    /* For now, identity mapping */
    return linux_keycode;
}

static const char unshifted_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char shifted_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

/* Modifier bits */
#define MOD_SHIFT 0x1
#define MOD_CTRL  0x2
#define MOD_ALT   0x4
#define MOD_SUPER 0x8
#define MOD_CAPSLOCK 0x10
#define MOD_NUMLOCK  0x20

static uint32_t get_unicode(int keycode, int modifiers) {
    if (keycode < 0 || keycode >= 128) return 0;
    
    int is_shift = (modifiers & MOD_SHIFT) != 0;
    int is_caps = (modifiers & MOD_CAPSLOCK) != 0;
    
    char c = 0;
    if (is_shift) {
        c = shifted_map[keycode];
    } else {
        c = unshifted_map[keycode];
    }
    
    /* Apply capslock for alphabetic characters */
    if (is_caps) {
        if (c >= 'a' && c <= 'z') c -= 32;
        else if (c >= 'A' && c <= 'Z') c += 32;
    }
    
    return (uint32_t)c;
}

int kb_init(keyboard_state_t *state)
{
    if (!state)
        return -1;
    memset(state, 0, sizeof(*state));
    return 0;
}

void kb_free(keyboard_state_t *state)
{
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
}

int kb_process_event(keyboard_state_t *state, const struct input_event *ev, key_event_t *out)
{
    if (!state || !ev || ev->type != EV_KEY)
        return -1;

    int keycode = linux_to_uos_keycode(ev->code);
    int pressed = ev->value; /* 0 = release, 1 = press, 2 = repeat */

    if (out) {
        out->keycode = keycode;
        out->unicode = get_unicode(keycode, state->modifiers);
        out->modifiers = state->modifiers;
    }

    /* Update state */
    if (pressed) {
        state->pressed[keycode] = 1;
        /* Update modifier state */
        switch (keycode) {
            case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT:
                state->modifiers |= MOD_SHIFT;
                break;
            case KEY_LEFTCTRL: case KEY_RIGHTCTRL:
                state->modifiers |= MOD_CTRL;
                break;
            case KEY_LEFTALT: case KEY_RIGHTALT:
                state->modifiers |= MOD_ALT;
                break;
            case KEY_LEFTMETA: case KEY_RIGHTMETA:
                state->modifiers |= MOD_SUPER;
                break;
            case KEY_CAPSLOCK:
                if (ev->value == 1) { /* press */
                    state->modifiers ^= MOD_CAPSLOCK; /* toggle */
                }
                break;
            case KEY_NUMLOCK:
                if (ev->value == 1) {
                    state->modifiers ^= MOD_NUMLOCK;
                }
                break;
            default:
                break;
        }
    } else {
        state->pressed[keycode] = 0;
        switch (keycode) {
            case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT:
                state->modifiers &= ~MOD_SHIFT;
                break;
            case KEY_LEFTCTRL: case KEY_RIGHTCTRL:
                state->modifiers &= ~MOD_CTRL;
                break;
            case KEY_LEFTALT: case KEY_RIGHTALT:
                state->modifiers &= ~MOD_ALT;
                break;
            case KEY_LEFTMETA: case KEY_RIGHTMETA:
                state->modifiers &= ~MOD_SUPER;
                break;
            default:
                break;
        }
    }

    return 0;
}

int kb_get_modifiers(const keyboard_state_t *state)
{
    if (!state)
        return 0;
    return state->modifiers;
}

int kb_set_leds(keyboard_state_t *state, int leds)
{
    /* Not implemented */
    return 0;
}

int kb_is_pressed(const keyboard_state_t *state, int keycode)
{
    if (!state || keycode < 0 || keycode >= KB_MAX_KEYS)
        return 0;
    return state->pressed[keycode];
}

void kb_set_repeat(int delay, int interval)
{
    /* System-wide, not implemented */
}