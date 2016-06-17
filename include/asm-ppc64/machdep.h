#ifdef __KERNEL__
#ifndef _PPC_MACHDEP_H
#define _PPC_MACHDEP_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/seq_file.h>
#include <linux/irq.h>

struct pt_regs;
struct pci_bus;	
struct pci_dev;
struct kbd_repeat;
struct device_node;
struct TceTable;
struct rtc_time;

struct machdep_calls {
	void            (*hpte_invalidate)(unsigned long slot,
					   unsigned long secondary,  
					   unsigned long va, 
					   int large, int local);
	long            (*hpte_updatepp)(unsigned long slot,
					 unsigned long secondary,  
					 unsigned long newpp, 
					 unsigned long va,
					 int large);
	void            (*hpte_updateboltedpp)(unsigned long newpp, 
					       unsigned long ea);
	long		(*hpte_insert)(unsigned long vpn,
				       unsigned long prpn,
				       unsigned long hpteflags, 
				       int bolted,
				       int large);
	long		(*hpte_remove)(unsigned long hpte_group);
	
	void		(*tce_build)(struct TceTable * tbl,
				     long tcenum,
				     unsigned long uaddr,
				     int direction);
	void		(*tce_free_one)(struct TceTable *tbl,
				        long tcenum);    

	void		(*smp_message_pass)(int target,
					    int msg, 
					    unsigned long data,
					    int wait);
	int		(*smp_probe)(void);
	void		(*smp_kick_cpu)(int nr);
	void		(*smp_setup_cpu)(int nr);

	void		(*setup_arch)(void);
	/* Optional, may be NULL. */
	void		(*setup_residual)(struct seq_file *m, int cpu_id);
	/* Optional, may be NULL. */
	void		(*get_cpuinfo)(struct seq_file *m);
	/* Optional, may be NULL. */
	unsigned int	(*irq_cannonicalize)(unsigned int irq);
	void		(*init_IRQ)(void);
	void		(*init_ras_IRQ)(void);
	void		(*init_irq_desc)(irq_desc_t *desc);
	int		(*get_irq)(struct pt_regs *);
	
	/* A general init function, called by ppc_init in init/main.c.
	   May be NULL. */
	void		(*init)(void);

	void		(*restart)(char *cmd);
	void		(*power_off)(void);
	void		(*halt)(void);

	long		(*time_init)(void); /* Optional, may be NULL */
	int		(*set_rtc_time)(struct rtc_time *);
	void		(*get_rtc_time)(struct rtc_time *);
	void		(*get_boot_time)(struct rtc_time *);
	void		(*calibrate_decr)(void);

  	void		(*progress)(char *, unsigned short);


	unsigned char 	(*nvram_read_val)(int addr);
	void		(*nvram_write_val)(int addr, unsigned char val);

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
#ifdef CONFIG_MAGIC_SYSRQ
	unsigned char 	*ppc_kbd_sysrq_xlate;
#endif

	/* Debug interface.  Low level I/O to some terminal device */
	void		(*udbg_putc)(unsigned char c);
	unsigned char	(*udbg_getc)(void);
	int		(*udbg_getc_poll)(void);

	/* PCI interfaces */
	int (*pcibios_read_config_byte)(struct device_node *dn, int offset, u8 *val);
	int (*pcibios_read_config_word)(struct device_node *dn, int offset, u16 *val);
	int (*pcibios_read_config_dword)(struct device_node *dn, int offset, u32 *val);
	int (*pcibios_write_config_byte)(struct device_node *dn, int offset, u8 val);
	int (*pcibios_write_config_word)(struct device_node *dn, int offset, u16 val);
	int (*pcibios_write_config_dword)(struct device_node *dn, int offset, u32 val);

	/* Called after scanning the bus, before allocating
	 * resources
	 */
	void (*pcibios_fixup)(void);

       /* Called for each PCI bus in the system
        * when it's probed
        */
	void (*pcibios_fixup_bus)(struct pci_bus *);

       /* Called when pci_enable_device() is called (initial=0) or
        * when a device with no assigned resource is found (initial=1).
        * Returns 0 to allow assignement/enabling of the device
        */
        int  (*pcibios_enable_device_hook)(struct pci_dev *, int initial);

	void* (*pci_dev_io_base)(unsigned char bus, unsigned char devfn, int physical);
	void* (*pci_dev_mem_base)(unsigned char bus, unsigned char devfn);
	int (*pci_dev_root_bridge)(unsigned char bus, unsigned char devfn);

	/* Interface for platform error logging */
	void (*log_error)(char *buf, unsigned int err_type, int fatal);

	/* this is for modules, since _machine can be a define -- Cort */
	int ppc_machine;
};

extern struct machdep_calls ppc_md;
extern char cmd_line[512];

extern void setup_pci_ptrs(void);


/* Functions to produce codes on the leds.
 * The SRC code should be unique for the message category and should
 * be limited to the lower 24 bits (the upper 8 are set by these funcs),
 * and (for boot & dump) should be sorted numerically in the order
 * the events occur.
 */
/* Print a boot progress message. */
void ppc64_boot_msg(unsigned int src, const char *msg);
/* Print a termination message (print only -- does not stop the kernel) */
void ppc64_terminate_msg(unsigned int src, const char *msg);
/* Print something that needs attention (device error, etc) */
void ppc64_attention_msg(unsigned int src, const char *msg);
/* Print a dump progress message. */
void ppc64_dump_msg(unsigned int src, const char *msg);

static inline void log_error(char *buf, unsigned int err_type, int fatal)
{
	if (ppc_md.log_error)
		ppc_md.log_error(buf, err_type, fatal);
}

#endif /* _PPC_MACHDEP_H */
#endif /* __KERNEL__ */
