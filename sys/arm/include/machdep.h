/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */
/* $FreeBSD$ */

#ifndef _MACHDEP_BOOT_MACHDEP_H_
#define _MACHDEP_BOOT_MACHDEP_H_

/* misc prototypes used by the many arm machdeps */
void arm_lock_cache_line(vm_offset_t);
void init_proc0(vm_offset_t kstack);
void halt(void);
void data_abort_handler(trapframe_t *);
void prefetch_abort_handler(trapframe_t *);
void undefinedinstruction_bounce(trapframe_t *);
struct arm_boot_params;
vm_offset_t default_parse_boot_param(struct arm_boot_params *abp);
vm_offset_t fake_preload_metadata(struct arm_boot_params *abp);
vm_offset_t parse_boot_param(struct arm_boot_params *abp);

#endif /* !_MACHINE_MACHDEP_H_ */
