/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */
/* $FreeBSD$ */

#ifndef _MACHDEP_BOOT_MACHDEP_H_
#define _MACHDEP_BOOT_MACHDEP_H_

/* Structs that need to be initialised by initarm */
struct pv_addr;
extern struct pv_addr irqstack;
extern struct pv_addr undstack;
extern struct pv_addr abtstack;

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#define UND_STACK_SIZE	1

/* misc prototypes used by the many arm machdeps */
struct trapframe;
void arm_lock_cache_line(vm_offset_t);
void init_proc0(vm_offset_t kstack);
void halt(void);
void data_abort_handler(struct trapframe *);
void prefetch_abort_handler(struct trapframe *);
void set_stackptrs(int cpu);
void undefinedinstruction_bounce(struct trapframe *);

/* Early boot related helper functions */
struct arm_boot_params;
vm_offset_t default_parse_boot_param(struct arm_boot_params *abp);
vm_offset_t freebsd_parse_boot_param(struct arm_boot_params *abp);
vm_offset_t linux_parse_boot_param(struct arm_boot_params *abp);
vm_offset_t fake_preload_metadata(struct arm_boot_params *abp);
vm_offset_t parse_boot_param(struct arm_boot_params *abp);

/*
 * Initialization functions called by the common initarm() function in
 * arm/machdep.c (but not necessarily from the custom initarm() functions of
 * older code).
 *
 *  - initarm_early_init() is called very early, after parsing the boot params
 *    and after physical memory has been located and sized.
 *
 *  - initarm_devmap_init() is called as one of the last steps of early virtual
 *    memory initialization, shortly before the new page tables are installed.
 *
 *  - initarm_lastaddr() is called after initarm_devmap_init(), and must return
 *    the address of the first byte of unusable KVA space.  This allows a
 *    platform to carve out of the top of the KVA space whatever reserves it
 *    needs for things like static device mapping, and this is called to get the
 *    value before calling pmap_bootstrap() which uses the value to size the
 *    available KVA.
 *
 *  - initarm_gpio_init() is called after the static device mappings are
 *    established and just before cninit().  The intention is that the routine
 *    can do any hardware setup (such as gpio or pinmux) necessary to make the
 *    console functional.
 *
 *  - initarm_late_init() is called just after cninit().  This is the first of
 *    the init routines that can use printf() and expect the output to appear on
 *    a standard console.
 *
 */
void initarm_early_init(void);
int initarm_devmap_init(void);
vm_offset_t initarm_lastaddr(void);
void initarm_gpio_init(void);
void initarm_late_init(void);

/* Board-specific attributes */
void board_set_serial(uint64_t);
void board_set_revision(uint32_t);

/* Setup standard arrays */
void arm_dump_avail_init( vm_offset_t memsize, size_t max);

#endif /* !_MACHINE_MACHDEP_H_ */
