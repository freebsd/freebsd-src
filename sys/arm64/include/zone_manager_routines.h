#ifndef _ZONE_MANAGER_ROUTINES_H_
#define _ZONE_MANAGER_ROUTINES_H_
#include <sys/types.h>
extern void zm_safe_restore_interrupts(register_t s);

extern void *zm_zone_enter(u_int32_t zone_id, void *aux);

#endif /* _ZONE_MANAGER_ROUTINES_H_ */