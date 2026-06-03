#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <linux/input.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KB_MAX_KEYS 256

typedef struct {
    int pressed[KB_MAX_KEYS];
    int modifiers; /* bitmask: 0x1=Shift, 0x2=Ctrl, 0x4=Alt, 0x8=Super, 0x10=CapsLock, 0x20=NumLock */
    int led_state; /* for keyboard LEDs */
} keyboard_state_t;

typedef struct {
    int keycode;   /* UOS keycode */
    int unicode;   /* generated unicode character, 0 if none */
    int modifiers; /* modifier state at time of event */
} key_event_t;

int kb_init(keyboard_state_t *state);
void kb_free(keyboard_state_t *state);
int kb_process_event(keyboard_state_t *state, const struct input_event *ev, key_event_t *out);
int kb_get_modifiers(const keyboard_state_t *state);
int kb_set_leds(keyboard_state_t *state, int leds); /* LED_NUMLOCK, LED_CAPSLock, LED_SCROLLLock */
int kb_is_pressed(const keyboard_state_t *state, int keycode);
void kb_set_repeat(int delay, int interval); /* system-wide repeat rate */

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_H */