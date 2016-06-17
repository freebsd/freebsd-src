/* drivers/message/fusion/linux_compat.h */

#ifndef FUSION_LINUX_COMPAT_H
#define FUSION_LINUX_COMPAT_H
/*{-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux/version.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/pci.h>

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/


#if (defined(__sparc__) && defined(__sparc_v9__)) || defined(__x86_64__)
#define MPT_CONFIG_COMPAT
#endif

#ifndef rwlock_init
#define rwlock_init(x) do { *(x) = RW_LOCK_UNLOCKED; } while(0)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#define SET_NICE(current,x)	do {(current)->nice = (x);} while (0)
#else
#define SET_NICE(current,x)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#define pci_enable_device(pdev)	(0)
#define SCSI_DATA_UNKNOWN	0
#define SCSI_DATA_WRITE		1
#define SCSI_DATA_READ		2
#define SCSI_DATA_NONE		3
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
#define pci_set_dma_mask(pdev, mask)	(0)
#define scsi_set_pci_device(sh, pdev)	(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
#	if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)
		typedef unsigned int dma_addr_t;
#	endif
#else
#	if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,42)
		typedef unsigned int dma_addr_t;
#	endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)
/*{-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

/* This block snipped from lk-2.2.18/include/linux/init.h { */
/*
 * Used for initialization calls..
 */
typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

#define __init_call	__attribute__ ((unused,__section__ (".initcall.init")))
#define __exit_call	__attribute__ ((unused,__section__ (".exitcall.exit")))

extern initcall_t __initcall_start, __initcall_end;

#define __initcall(fn)								\
	static initcall_t __initcall_##fn __init_call = fn
#define __exitcall(fn)								\
	static exitcall_t __exitcall_##fn __exit_call = fn

#ifdef MODULE
/* These macros create a dummy inline: gcc 2.9x does not count alias
 as usage, hence the `unused function' warning when __init functions
 are declared static. We use the dummy __*_module_inline functions
 both to kill the warning and check the type of the init/cleanup
 function. */
typedef int (*__init_module_func_t)(void);
typedef void (*__cleanup_module_func_t)(void);
#define module_init(x) \
	int init_module(void) __attribute__((alias(#x))); \
	static inline __init_module_func_t __init_module_inline(void) \
	{ return x; }
#define module_exit(x) \
	void cleanup_module(void) __attribute__((alias(#x))); \
	static inline __cleanup_module_func_t __cleanup_module_inline(void) \
	{ return x; }

#else
#define module_init(x)	__initcall(x);
#define module_exit(x)	__exitcall(x);
#endif
/* } block snipped from lk-2.2.18/include/linux/init.h */

/* This block snipped from lk-2.2.18/include/linux/sched.h { */
/*
 * Used prior to schedule_timeout calls..
 */
#define __set_current_state(state_value)	do { current->state = state_value; } while (0)
#ifdef CONFIG_SMP
#define set_current_state(state_value)		do { __set_current_state(state_value); mb(); } while (0)
#else
#define set_current_state(state_value)		__set_current_state(state_value)
#endif
/* } block snipped from lk-2.2.18/include/linux/sched.h */

/* procfs compat stuff... */
#define proc_mkdir(x,y)			create_proc_entry(x, S_IFDIR, y)

/* MUTEX compat stuff... */
#define DECLARE_MUTEX(name)		struct semaphore name=MUTEX
#define DECLARE_MUTEX_LOCKED(name)	struct semaphore name=MUTEX_LOCKED
#define init_MUTEX(x)			*(x)=MUTEX
#define init_MUTEX_LOCKED(x)		*(x)=MUTEX_LOCKED

/* Wait queues. */
#define DECLARE_WAIT_QUEUE_HEAD(name)	\
	struct wait_queue * (name) = NULL
#define DECLARE_WAITQUEUE(name, task)	\
	struct wait_queue (name) = { (task), NULL }

#if defined(__sparc__) && defined(__sparc_v9__)
/* The sparc64 ioremap implementation is wrong in 2.2.x,
 * but fixing it would break all of the drivers which
 * workaround it.  Fixed in 2.3.x onward. -DaveM
 */
#define ARCH_IOREMAP(base)	((unsigned long) (base))
#else
#define ARCH_IOREMAP(base)	ioremap(base)
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#else		/* LINUX_VERSION_CODE must be >= KERNEL_VERSION(2,2,18) */

/* No ioremap bugs in >2.3.x kernels. */
#define ARCH_IOREMAP(base)	ioremap(base)

/*}-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#endif		/* LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18) */


/*
 * Inclined to use:
 *   #if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10)
 * here, but MODULE_LICENSE defined in 2.4.9-6 and 2.4.9-13
 * breaks the rule:-(
 */
#ifndef MODULE_LICENSE
#define MODULE_LICENSE(license)
#endif


/* PCI/driver subsystem { */
#ifndef pci_for_each_dev
#define pci_for_each_dev(dev)		for((dev)=pci_devices; (dev)!=NULL; (dev)=(dev)->next)
#define pci_peek_next_dev(dev)		((dev)->next ? (dev)->next : NULL)
#define DEVICE_COUNT_RESOURCE           6
#define PCI_BASEADDR_FLAGS(idx)         base_address[idx]
#define PCI_BASEADDR_START(idx)         base_address[idx] & ~0xFUL
/*
 * We have to keep track of the original value using
 * a temporary, and not by just sticking pdev->base_address[x]
 * back.  pdev->base_address[x] is an opaque cookie that can
 * be used by the PCI implementation on a given Linux port
 * for any purpose. -DaveM
 */
#define PCI_BASEADDR_SIZE(__pdev, __idx) \
({	unsigned int size, tmp; \
	pci_read_config_dword(__pdev, PCI_BASE_ADDRESS_0 + (4*(__idx)), &tmp); \
	pci_write_config_dword(__pdev, PCI_BASE_ADDRESS_0 + (4*(__idx)), 0xffffffff); \
	pci_read_config_dword(__pdev, PCI_BASE_ADDRESS_0 + (4*(__idx)), &size); \
	pci_write_config_dword(__pdev, PCI_BASE_ADDRESS_0 + (4*(__idx)), tmp); \
	(4 - size); \
})
#else
#define pci_peek_next_dev(dev)		((dev) != pci_dev_g(&pci_devices) ? pci_dev_g((dev)->global_list.next) : NULL)
#define PCI_BASEADDR_FLAGS(idx)         resource[idx].flags
#define PCI_BASEADDR_START(idx)         resource[idx].start
#define PCI_BASEADDR_SIZE(dev,idx)      (dev)->resource[idx].end - (dev)->resource[idx].start + 1
#endif		/* } ifndef pci_for_each_dev */


/* Compatability for the 2.3.x PCI DMA API. */
#ifndef PCI_DMA_BIDIRECTIONAL
/*{-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#define PCI_DMA_BIDIRECTIONAL	0
#define PCI_DMA_TODEVICE	1
#define PCI_DMA_FROMDEVICE	2
#define PCI_DMA_NONE		3

#ifdef __KERNEL__
#include <asm/page.h>
/* Pure 2^n version of get_order */
static __inline__ int __get_order(unsigned long size)
{
	int order;

	size = (size-1) >> (PAGE_SHIFT-1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}
#endif

#define pci_alloc_consistent(hwdev, size, dma_handle) \
({	void *__ret = (void *)__get_free_pages(GFP_ATOMIC, __get_order(size)); \
	if (__ret != NULL) { \
		memset(__ret, 0, size); \
		*(dma_handle) = virt_to_bus(__ret); \
	} \
	__ret; \
})

#define pci_free_consistent(hwdev, size, vaddr, dma_handle) \
	free_pages((unsigned long)vaddr, __get_order(size))

#define pci_map_single(hwdev, ptr, size, direction) \
	virt_to_bus(ptr);

#define pci_unmap_single(hwdev, dma_addr, size, direction) \
	do { /* Nothing to do */ } while (0)

#define pci_map_sg(hwdev, sg, nents, direction)	(nents)
#define pci_unmap_sg(hwdev, sg, nents, direction) \
	do { /* Nothing to do */ } while(0)

#define sg_dma_address(sg)	(virt_to_bus((sg)->address))
#define sg_dma_len(sg)		((sg)->length)

/*}-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#endif /* PCI_DMA_BIDIRECTIONAL */

/*
 *  With the new command queuing code in the SCSI mid-layer we no longer have
 *  to hold the io_request_lock spin lock when calling the scsi_done routine.
 *  For now we only do this with the 2.5.1 kernel or newer.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,1)
        #define MPT_HOST_LOCK(flags)
        #define MPT_HOST_UNLOCK(flags)
#else
        #define MPT_HOST_LOCK(flags) \
                spin_lock_irqsave(&io_request_lock, flags)
        #define MPT_HOST_UNLOCK(flags) \
                spin_unlock_irqrestore(&io_request_lock, flags)
#endif

/*
 *  We use our new error handling code if the kernel version is 2.4.18 or newer.
 *  Remark: 5/5/03 use old EH code with 2.4 kernels as it runs in a background thread
 *  2.4 kernels choke on a call to schedule via eh thread.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,1)
        #define MPT_SCSI_USE_NEW_EH
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,41)
#define mpt_work_struct work_struct
#define MPT_INIT_WORK(_task, _func, _data) INIT_WORK(_task, _func, _data)
#else
#define mpt_work_struct tq_struct
#define MPT_INIT_WORK(_task, _func, _data) \
({	(_task)->sync = 0; \
	(_task)->routine = (_func); \
	(_task)->data = (void *) (_data); \
})
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,28)
#define mpt_sync_irq(_irq) synchronize_irq(_irq)
#else
#define mpt_sync_irq(_irq) synchronize_irq()
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,58)
#define mpt_inc_use_count()
#define mpt_dec_use_count()
#else
#define mpt_inc_use_count() MOD_INC_USE_COUNT
#define mpt_dec_use_count() MOD_DEC_USE_COUNT
#endif


/*}-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#endif /* _LINUX_COMPAT_H */

