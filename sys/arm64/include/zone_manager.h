#ifndef _ZONE_MANAGER_H_
#define _ZONE_MANAGER_H_

#include <sys/types.h>
#include <machine/zone_manager_asm.h>
#define ZM_RO __attribute__((section(".zm_ro")))

typedef void * (*zm_zone_dispatch_fcn_t)(void *);


typedef struct zm_pcpu {
    /** The privileged stacks for each zone */
    uintptr_t zone_stacks[ZONE_STATE_POS_COUNT];
    /** One of ZONE_STATE_xxx indicating the state of this CPU's zone manager */
    int32_t state;
    /**
     * Unique identity of the owning CPU.
     * We locate the correct secure PCPU using the insecure PCPU's index value.
     * This index may be completely bogus and point to the wrong secure PCPU.
     * Detect this condition and abort. 
     */
    uint32_t mpidr;
} zm_pcpu_s;

/** 
 * The globals structure holds all of the protected globals for the zone
 * manager. These must be placed in a structure rather than, say, BSS because
 * we need to protect the globals under a watchpoint. We share the pmap's
 * watchpoint due to mutually assured destruction.
 */
typedef struct zm_globals {
    /** 
     * Indicates that a panic was called by any zone member.
     * Prevents zone re-entry. 
     */
    u_int32_t is_panicked;
} zm_globals_s;

typedef struct zm_init_watchpoint_config {
    u_int64_t wcr;
    u_int64_t wvr;
} zm_init_watchpoint_config_s;

extern zm_globals_s *zm_globals;
extern zm_pcpu_s *zm_pcpus;
extern u_int32_t zm_pcpu_count;
extern const zm_zone_dispatch_fcn_t zm_dispatch_functions[];

#endif /* _ZONE_MANAGER_H_ */