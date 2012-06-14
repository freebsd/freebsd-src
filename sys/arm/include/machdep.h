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

/* Early boot related helper functions */
struct arm_boot_params;
vm_offset_t default_parse_boot_param(struct arm_boot_params *abp);
vm_offset_t freebsd_parse_boot_param(struct arm_boot_params *abp);
vm_offset_t linux_parse_boot_param(struct arm_boot_params *abp);
vm_offset_t fake_preload_metadata(struct arm_boot_params *abp);
vm_offset_t parse_boot_param(struct arm_boot_params *abp);

/* Setup standard arrays */
void arm_dump_avail_init( vm_offset_t memsize, size_t max);

#endif /* !_MACHINE_MACHDEP_H_ */
