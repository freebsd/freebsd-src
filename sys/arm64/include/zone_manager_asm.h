#ifndef _ZONE_MANAGER_ASM_H_
#define _ZONE_MANAGER_ASM_H_
#define ZONE_STATE_PMAP         0
#define ZONE_STATE_CAPABILITIES 1
#define ZONE_STATE_POS_MAX ZONE_STATE_CAPABILITIES
#define ZONE_STATE_POS_COUNT (ZONE_STATE_POS_MAX + 1)
/* Signal states, do not correspond to real zones */
/** Indicates this CPU is not in a zone; that is, we are in kernel mode */
#define ZONE_STATE_NONE         -1
#define ZONE_STATE_EXCEPTION    -2

#endif /* _ZONE_MANAGER_ASM_H_ */