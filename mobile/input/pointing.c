#include "pointing.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

int ptr_init(ptr_state_t *state)
{
    if (!state)
        return -1;
    memset(state, 0, sizeof(*state));
    return 0;
}

void ptr_free(ptr_state_t *state)
{
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
}

int ptr_process_event(ptr_state_t *state, int rel_x, int rel_y, int buttons, int wheel, int hwheel, const ptr_config_t *config)
{
    if (!state)
        return -1;

    /* Apply acceleration */
    if (config) {
        float accel = 1.0;
        switch (config->profile) {
            case PTR_ACCEL_LINEAR:
                accel = 1.0 + config->acceleration * (sqrt(fabs(rel_x)) + sqrt(fabs(rel_y))) / 2.0;
                break;
            case PTR_ACCEL_EASED:
                accel = 1.0 + config->acceleration * (1.0 - exp(-(fabs(rel_x) + fabs(rel_y)) / config->threshold));
                break;
            case PTR_ACCEL_OS_STYLE:
                /* Placeholder */
                accel = 1.0 + config->acceleration;
                break;
        }
        rel_x = (int)(rel_x * accel);
        rel_y = (int)(rel_y * accel);
    }

    state->x += rel_x;
    state->y += rel_y;
    state->buttons = buttons;
    state->wheel += wheel;
    state->hwheel += hwheel;

    return 0;
}

void ptr_get_state(const ptr_state_t *state, int *x, int *y, int *buttons, int *wheel, int *hwheel)
{
    if (!state)
        return;
    if (x) *x = state->x;
    if (y) *y = state->y;
    if (buttons) *buttons = state->buttons;
    if (wheel) *wheel = state->wheel;
    if (hwheel) *hwheel = state->hwheel;
}

void ptr_set_accel_profile(ptr_state_t *state, ptr_accel_profile_t profile)
{
    /* Not implemented, would need to store config in state */
}

int ptr_is_button_pressed(const ptr_state_t *state, int button)
{
    if (!state)
        return 0;
    return (state->buttons & button) != 0;
}