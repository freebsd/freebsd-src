#ifndef POINTING_H
#define POINTING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PTR_ACCEL_LINEAR,
    PTR_ACCEL_EASED,
    PTR_ACCEL_OS_STYLE
} ptr_accel_profile_t;

typedef struct {
    int x;
    int y;
    int buttons; /* bitmask: 0x1=left, 0x2=right, 0x4=middle, 0x8=back, 0x10=forward */
    int wheel;   /* vertical wheel */
    int hwheel;  /* horizontal wheel */
} ptr_state_t;

typedef struct {
    ptr_accel_profile_t profile;
    float acceleration; /* acceleration factor */
    float threshold;    /* threshold for acceleration */
} ptr_config_t;

int ptr_init(ptr_state_t *state);
void ptr_free(ptr_state_t *state);
int ptr_process_event(ptr_state_t *state, int rel_x, int rel_y, int buttons, int wheel, int hwheel, const ptr_config_t *config);
void ptr_get_state(const ptr_state_t *state, int *x, int *y, int *buttons, int *wheel, int *hwheel);
void ptr_set_accel_profile(ptr_state_t *state, ptr_accel_profile_t profile);
int ptr_is_button_pressed(const ptr_state_t *state, int button);

#ifdef __cplusplus
}
#endif

#endif /* POINTING_H */