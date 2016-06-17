#ifdef __KERNEL__
#ifndef _PPC_MACHDEP_H
#define _PPC_MACHDEP_H

#include <linux/config.h>

#ifdef CONFIG_APUS
#include <asm-m68k/machdep.h>
#endif

struct pt_regs;
struct pci_bus;
struct pci_dev;
struct seq_file;

/* We export this macro for external modules like Alsa to know if
 * ppc_md.feature_call is implemented or not
 */
#define CONFIG_PPC_HAS_FEATURE_CALLS

struct machdep_calls {
	void		(*setup_arch)(void);
	/* Optional, may be NULL. */
	int		(*show_cpuinfo)(struct seq_file *m);
	int		(*show_percpuinfo)(struct seq_file *m, int i);
	/* Optional, may be NULL. */
	unsigned int	(*irq_cannonicalize)(unsigned int irq);
	void		(*init_IRQ)(void);
	int		(*get_irq)(struct pt_regs *);

	/* A general init function, called by ppc_init in init/main.c.
	   May be NULL. */
	void		(*init)(void);

	void		(*restart)(char *cmd);
	void		(*power_off)(void);
	void		(*halt)(void);

	long		(*time_init)(void); /* Optional, may be NULL */
	int		(*set_rtc_time)(unsigned long nowtime);
	unsigned long	(*get_rtc_time)(void);
	void		(*calibrate_decr)(void);
	void		(*heartbeat)(void);

	unsigned long	(*find_end_of_memory)(void);
	void		(*setup_io_mappings)(void);

	void		(*early_serial_map)(void);
  	void		(*progress)(char *, unsigned short);

	unsigned char 	(*nvram_read_val)(int addr);
	void		(*nvram_write_val)(int addr, unsigned char val);

	/* Called from prepare_namespace() */
	void		(*discover_root)(void);

	/* Tons of keyboard stuff. */
	int		(*kbd_setkeycode)(unsigned int scancode,
				unsigned int keycode);
	int		(*kbd_getkeycode)(unsigned int scancode);
	int		(*kbd_translate)(unsigned char scancode,
				unsigned char *keycode,
				char raw_mode);
	char		(*kbd_unexpected_up)(unsigned char keycode);
	void		(*kbd_leds)(unsigned char leds);
	void		(*kbd_init_hw)(void);
	unsigned char 	*ppc_kbd_sysrq_xlate;

	/*
	 * optional PCI "hooks"
	 */

	/* Called after scanning the bus, before allocating resources */
	void (*pcibios_fixup)(void);

	/* Called after PPC generic resource fixup to perform
	   machine specific fixups */
	void (*pcibios_fixup_resources)(struct pci_dev *);

	/* Called for each PCI bus in the system when it's probed */
	void (*pcibios_fixup_bus)(struct pci_bus *);

	/* Called when pci_enable_device() is called (initial=0) or
	 * when a device with no assigned resource is found (initial=1).
	 * Returns 0 to allow assignment/enabling of the device. */
	int  (*pcibios_enable_device_hook)(struct pci_dev *, int initial);

	/* For interrupt routing */
	unsigned char (*pci_swizzle)(struct pci_dev *, unsigned char *);
	int (*pci_map_irq)(struct pci_dev *, unsigned char, unsigned char);

	/* Called in indirect_* to avoid touching devices */
	int (*pci_exclude_device)(unsigned char, unsigned char);

	/* Called at then very end of pcibios_init() */
	void (*pcibios_after_init)(void);

	/* this is for modules, since _machine can be a define -- Cort */
	int ppc_machine;

	/* Motherboard/chipset features. This is a kind of general purpose
	 * hook used to control some machine specific features (like reset
	 * lines, chip power control, etc...).
	 */
	int (*feature_call)(unsigned int feature, ...);

	/* Hook for board-specific info passed by the bootloader */
	void (*board_info)(void *bdinfo, int bdinfo_size);

#ifdef CONFIG_SMP
	/* functions for dealing with other cpus */
	struct smp_ops_t *smp_ops;
#endif /* CONFIG_SMP */
};

extern struct machdep_calls ppc_md;
extern char cmd_line[];

extern void setup_pci_ptrs(void);

/*
 * Power macintoshes have either a CUDA or a PMU controlling
 * system reset, power, NVRAM, RTC.
 */
typedef enum sys_ctrler_kind {
	SYS_CTRLER_UNKNOWN = 0,
	SYS_CTRLER_CUDA = 1,
	SYS_CTRLER_PMU = 2,
} sys_ctrler_t;

extern sys_ctrler_t sys_ctrler;

#ifdef CONFIG_SMP
struct smp_ops_t {
	void  (*message_pass)(int target, int msg, unsigned long data, int wait);
	int   (*probe)(void);
	void  (*kick_cpu)(int nr);
	void  (*setup_cpu)(int nr);
};
#endif /* CONFIG_SMP */

#endif /* _PPC_MACHDEP_H */
#endif /* __KERNEL__ */
