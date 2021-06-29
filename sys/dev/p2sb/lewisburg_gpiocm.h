#ifndef __LEWISBURG_GPIOCM_H__
#define __LEWISBURG_GPIOCM_H__

int lbggpiocm_get_group_npins(device_t dev, device_t child);
char lbggpiocm_get_group_name(device_t dev, device_t child);

int lbggpiocm_pin_setflags(device_t, device_t, uint32_t, uint32_t);
int lbggpiocm_pin_get(device_t, device_t, uint32_t, uint32_t *);
int lbggpiocm_pin_set(device_t, device_t, uint32_t, uint32_t);
int lbggpiocm_pin_toggle(device_t, device_t, uint32_t);

#endif /* __LEWISBURG_GPIOCM_H__ */
