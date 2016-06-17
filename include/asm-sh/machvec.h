/*
 * include/asm-sh/machvec.h
 *
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef _ASM_SH_MACHVEC_H
#define _ASM_SH_MACHVEC_H 1

#include <linux/config.h>
#include <linux/types.h>

struct timeval;

struct sh_machine_vector
{
	const char *mv_name;

	int mv_nr_irqs;

	unsigned char (*mv_inb)(unsigned long);
	unsigned short (*mv_inw)(unsigned long);
	unsigned int (*mv_inl)(unsigned long);
	void (*mv_outb)(unsigned char, unsigned long);
	void (*mv_outw)(unsigned short, unsigned long);
	void (*mv_outl)(unsigned int, unsigned long);

	unsigned char (*mv_inb_p)(unsigned long);
	unsigned short (*mv_inw_p)(unsigned long);
	unsigned int (*mv_inl_p)(unsigned long);
	void (*mv_outb_p)(unsigned char, unsigned long);
	void (*mv_outw_p)(unsigned short, unsigned long);
	void (*mv_outl_p)(unsigned int, unsigned long);

	void (*mv_insb)(unsigned long port, void *addr, unsigned long count);
	void (*mv_insw)(unsigned long port, void *addr, unsigned long count);
	void (*mv_insl)(unsigned long port, void *addr, unsigned long count);
	void (*mv_outsb)(unsigned long port, const void *addr, unsigned long count);
	void (*mv_outsw)(unsigned long port, const void *addr, unsigned long count);
	void (*mv_outsl)(unsigned long port, const void *addr, unsigned long count);

	unsigned char (*mv_readb)(unsigned long);
	unsigned short (*mv_readw)(unsigned long);
	unsigned int (*mv_readl)(unsigned long);
	void (*mv_writeb)(unsigned char, unsigned long);
	void (*mv_writew)(unsigned short, unsigned long);
	void (*mv_writel)(unsigned int, unsigned long);

	void* (*mv_ioremap)(unsigned long offset, unsigned long size);
	void (*mv_iounmap)(void *addr);

	unsigned long (*mv_isa_port2addr)(unsigned long offset);

	int (*mv_irq_demux)(int irq);

	void (*mv_init_arch)(void);
	void (*mv_init_irq)(void);
	void (*mv_init_pci)(void);
	void (*mv_kill_arch)(int);

	void (*mv_heartbeat)(void);

	void (*mv_rtc_gettimeofday)(struct timeval *tv);
	int (*mv_rtc_settimeofday)(const struct timeval *tv);

	unsigned int mv_hw_se : 1;
	unsigned int mv_hw_shmse : 1;
	unsigned int mv_hw_hp600 : 1;
	unsigned int mv_hw_hp620 : 1;
	unsigned int mv_hw_hp680 : 1;
	unsigned int mv_hw_hp690 : 1;
	unsigned int mv_hw_hd64461 : 1;
	unsigned int mv_hw_sh2000 : 1;
	unsigned int mv_hw_hd64465 : 1;
	unsigned int mv_hw_dreamcast : 1;
	unsigned int mv_hw_bigsur : 1;
	unsigned int mv_hw_hs7729pci : 1;
	unsigned int mv_hw_7751se: 1;
	unsigned int mv_hw_adx : 1;
	unsigned int mv_hw_snapgear : 1;
	unsigned int mv_hw_sh4202_microdev : 1;
};

extern struct sh_machine_vector sh_mv;

/* Machine check macros */
#ifdef CONFIG_SH_GENERIC
#define MACH_SE		(sh_mv.mv_hw_se)
#define MACH_SHMSE	(sh_mv.mv_hw_shmse)
#define MACH_HP600	(sh_mv.mv_hw_hp600)
#define MACH_HP620	(sh_mv.mv_hw_hp620)
#define MACH_HP680	(sh_mv.mv_hw_hp680)
#define MACH_HP690	(sh_mv.mv_hw_hp690)
#define MACH_HD64461	(sh_mv.mv_hw_hd64461)
#define MACH_HD64465	(sh_mv.mv_hw_hd64465)
#define MACH_SH2000	(sh_mv.mv_hw_sh2000)
#define MACH_DREAMCAST	(sh_mv.mv_hw_dreamcast)
#define MACH_BIGSUR	(sh_mv.mv_hw_bigsur)
#define MACH_HS7729PCI	(sh_mv.mv_hw_hs7729pci)
#define MACH_7751SE	(sh_mv.mv_hw_7751se)
#define MACH_ADX	(sh_mv.mv_hw_adx)
#define MACH_SNAPGEAR	(sh_mv.mv_snapgear)
#define MACH_SH4202_MICRODEV	(sh_mv.mv_hw_sh4202_microdev)
#else
# ifdef CONFIG_SH_SOLUTION_ENGINE
#  define MACH_SE		1
# else
#  define MACH_SE		0
# endif
# ifdef CONFIG_SH_7751_SOLUTION_ENGINE
#  define MACH_7751SE		1
# else
#  define MACH_7751SE		0
# endif
# ifdef CONFIG_SH_MOBILE_SOLUTION_ENGINE
#  define MACH_SHMSE 	        1
# else
#  define MACH_SHMSE		0
# endif
# ifdef CONFIG_SH_HP600
#  define MACH_HP600		1
# else
#  define MACH_HP600		0
# endif
# ifdef CONFIG_SH_HP620
#  define MACH_HP620		1
# else
#  define MACH_HP620		0
# endif
# ifdef CONFIG_SH_HP680
#  define MACH_HP680		1
# else
#  define MACH_HP680		0
# endif
# ifdef CONFIG_SH_HP690
#  define MACH_HP690		1
# else
#  define MACH_HP690		0
# endif
# ifdef CONFIG_HD64461
#  define MACH_HD64461		1
# else
#  define MACH_HD64461		0
# endif
# ifdef CONFIG_HD64465
#  define MACH_HD64465		1
# else
#  define MACH_HD64465		0
# endif
# ifdef CONFIG_SH_SH2000
#  define MACH_SH2000		1
# else
#  define MACH_SH2000		0
# endif
# ifdef CONFIG_SH_EC3104
#  define MACH_EC3104		1
# else
#  define MACH_EC3104		0
# endif
# ifdef CONFIG_SH_DREAMCAST
#  define MACH_DREAMCAST	1
# else
#  define MACH_DREAMCAST	0
# endif
# ifdef CONFIG_SH_BIGSUR
#  define MACH_BIGSUR		1
# else
#  define MACH_BIGSUR		0
# endif
# ifdef CONFIG_SH_HS7729PCI
#  define MACH_HS7729PCI	1
# else
#  define MACH_HS7729PCI	0
# endif
# ifdef CONFIG_SH_ADX
#  define MACH_ADX		1
# else
#  define MACH_ADX		0
# endif
# ifdef CONFIG_SH_SECUREEDGE5410
#  define MACH_SNAPGEAR		1
# else
#  define MACH_SNAPGEAR		0
# endif
# ifdef CONFIG_SH_SH4202_MICRODEV
#  define MACH_SH4202_MICRODEV	1
# else
#  define MACH_SH4202_MICRODEV	0
# endif
#endif

#endif /* _ASM_SH_MACHVEC_H */
