/*
 * Mobile UI System Main Interface
 * uOS(m) - User OS Mobile
 */

#ifndef _MOBILE_UI_H_
#define _MOBILE_UI_H_

#include <stdint.h>
#include "window_manager.h"
#include "ui_widget.h"
#include "framebuffer.h"

/* UI system states */
typedef enum {
    UI_STATE_INIT,
    UI_STATE_RUNNING,
    UI_STATE_SUSPENDED,
    UI_STATE_SHUTDOWN
} ui_state_t;

/* Initialize mobile UI system */
int mobile_ui_init(void);

/* Start UI system */
int mobile_ui_start(void);

/* Stop UI system */
void mobile_ui_stop(void);

/* Get UI system state */
ui_state_t mobile_ui_get_state(void);

/* Main UI event loop (called by kernel) */
void mobile_ui_event_loop(void);

/* Input handling */
void mobile_ui_handle_touch(int x, int y, touch_action_t action);
void mobile_ui_handle_key(int keycode, int pressed);

/* Screen management */
void mobile_ui_set_brightness(int brightness);
int mobile_ui_get_brightness(void);

/* Demo functions */
void mobile_ui_create_demo(void);
void mobile_ui_demo_animations(void);

#endif /* _MOBILE_UI_H_ */