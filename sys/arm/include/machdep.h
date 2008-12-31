/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */
/* $FreeBSD: src/sys/arm/include/machdep.h,v 1.2.10.1.2.1 2008/11/25 02:59:29 kensmith Exp $ */

#ifndef _MACHDEP_BOOT_MACHDEP_H_
#define _MACHDEP_BOOT_MACHDEP_H_

/* misc prototypes used by the many arm machdeps */
void arm_lock_cache_line(vm_offset_t);
vm_offset_t fake_preload_metadata(void);
void halt(void);
void data_abort_handler(trapframe_t *);
void prefetch_abort_handler(trapframe_t *);
void undefinedinstruction_bounce(trapframe_t *);

#endif /* !_MACHINE_MACHDEP_H_ */
