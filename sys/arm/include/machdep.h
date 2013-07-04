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

/* Called by initarm */
vm_offset_t initarm_lastaddr(void);
void initarm_gpio_init(void);
void initarm_late_init(void);
int platform_devmap_init(void);

/* Board-specific attributes */
void board_set_serial(uint64_t);
void board_set_revision(uint32_t);

/* Needs to be initialised by platform_devmap_init */
extern const struct pmap_devmap *pmap_devmap_bootstrap_table;

/* Setup standard arrays */
void arm_dump_avail_init( vm_offset_t memsize, size_t max);

#endif /* !_MACHINE_MACHDEP_H_ */
