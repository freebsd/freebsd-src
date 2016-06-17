/*
 * ACPI sytle PM for SMP AMD-760MP(X) based systems.
 * For use until the ACPI project catches up. :-)
 *
 * Copyright (C) 2002 Johnathan Hicks <thetech@folkwolf.net>
 *
 * History:
 * 
 *   20020702 - amd-smp-idle: Tony Lindgren <tony@atomide.com>
 *	Influenced by Vcool, and LVCool. Rewrote everything from scratch to
 *	use the PCI features in Linux, and to support SMP systems. Provides
 *	C2 idling on SMP AMD-760MP systems.
 *	
 *   20020722: JH
 *   	I adapted Tony's code for the AMD-765/766 southbridge and adapted it
 *   	according to the AMD-768 data sheet to provide the same capability for
 *   	SMP AMD-760MPX systems. Posted to acpi-devel list.
 *   	
 *   20020722: Alan Cox
 *   	Replaces non-functional amd76x_pm code in -ac tree.
 *   	
 *   20020730: JH
 *   	Added ability to do normal throttling (the non-thermal kind), C3 idling
 *   	and Power On Suspend (S1 sleep). It would be very easy to tie swsusp
 *   	into activate_amd76x_SLP(). C3 idling doesn't happen yet; see my note
 *   	in amd76x_smp_idle(). I've noticed that when NTH and idling are both
 *   	enabled, my hardware locks and requires a hard reset, so I have
 *   	#ifndefed around the idle loop setting to prevent this. POS locks it up
 *   	too, both ought to be fixable. I've also noticed that idling and NTH
 *   	make some interference that is picked up by the onboard sound chip on
 *   	my ASUS A7M266-D motherboard.
 *
 *
 * TODO: Thermal throttling (TTH).
 * 	 /proc interface for normal throttling level.
 * 	 /proc interface for POS.
 *
 *
 *    <Notes from 20020722-ac revision>
 *
 * Processor idle mode module for AMD SMP 760MP(X) based systems
 *
 * Copyright (C) 2002 Tony Lindgren <tony@atomide.com>
 *                    Johnathan Hicks (768 support)
 *
 * Using this module saves about 70 - 90W of energy in the idle mode compared
 * to the default idle mode. Waking up from the idle mode is fast to keep the
 * system response time good. Currently no CPU load calculation is done, the
 * system exits the idle mode if the idle function runs twice on the same
 * processor in a row. This only works on SMP systems, but maybe the idle mode
 * enabling can be integrated to ACPI to provide C2 mode at some point.
 *
 * NOTE: Currently there's a bug somewhere where the reading the
 *       P_LVL2 for the first time causes the system to sleep instead of 
 *       idling. This means that you need to hit the power button once to
 *       wake the system after loading the module for the first time after
 *       reboot. After that the system idles as supposed.
 *
 *
 * Influenced by Vcool, and LVCool. Rewrote everything from scratch to
 * use the PCI features in Linux, and to support SMP systems.
 * 
 * Currently only tested on a TYAN S2460 (760MP) system (Tony) and an
 * ASUS A7M266-D (760MPX) system (Johnathan). Adding support for other Athlon
 * SMP or single processor systems should be easy if desired.
 *
 * This software is licensed under GNU General Public License Version 2 
 * as specified in file COPYING in the Linux kernel source tree main 
 * directory.
 * 
 *   </Notes from 20020722-ac revision>
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include "amd76x_pm.h"

#define VERSION	"20020730"

// #define AMD76X_C3  1
// #define AMD76X_NTH 1
// #define AMD76X_POS 1


extern void default_idle(void);
static void amd76x_smp_idle(void);
static int amd76x_pm_main(void);
static int __devinit amd_nb_init(struct pci_dev *pdev,
				 const struct pci_device_id *ent);
static void amd_nb_remove(struct pci_dev *pdev);
static int __devinit amd_sb_init(struct pci_dev *pdev,
				 const struct pci_device_id *ent);
static void amd_sb_remove(struct pci_dev *pdev);


static struct pci_dev *pdev_nb;
static struct pci_dev *pdev_sb;

struct PM_cfg {
	unsigned int status_reg;
	unsigned int C2_reg;
	unsigned int C3_reg;
	unsigned int NTH_reg;
	unsigned int slp_reg;
	unsigned int resume_reg;
	void (*orig_idle) (void);
	void (*curr_idle) (void);
	unsigned long C2_cnt, C3_cnt;
	int last_pr;
};
static struct PM_cfg amd76x_pm_cfg;

struct cpu_idle_state {
	int idle;
	int count;
};
static struct cpu_idle_state prs[2];

static struct pci_device_id amd_nb_tbl[] __devinitdata = {
	{PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_FE_GATE_700C, PCI_ANY_ID, PCI_ANY_ID,},
	{0,}
};

static struct pci_device_id amd_sb_tbl[] __devinitdata = {
	{PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_7413, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_7443, PCI_ANY_ID, PCI_ANY_ID,},
	{0,}
};

static struct pci_driver amd_nb_driver = {
	name:"amd76x_pm-nb",
	id_table:amd_nb_tbl,
	probe:amd_nb_init,
	remove:__devexit_p(amd_nb_remove),
};

static struct pci_driver amd_sb_driver = {
	name:"amd76x_pm-sb",
	id_table:amd_sb_tbl,
	probe:amd_sb_init,
	remove:__devexit_p(amd_sb_remove),
};


static int __devinit
amd_nb_init(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	pdev_nb = pdev;
	printk(KERN_INFO "amd76x_pm: Initializing northbridge %s\n",
	       pdev_nb->name);

	return 0;
}


static void __devexit
amd_nb_remove(struct pci_dev *pdev)
{
}


static int __devinit
amd_sb_init(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	pdev_sb = pdev;
	printk(KERN_INFO "amd76x_pm: Initializing southbridge %s\n",
	       pdev_sb->name);

	return 0;
}


static void __devexit
amd_sb_remove(struct pci_dev *pdev)
{
}


/*
 * Configures the AMD-762 northbridge to support PM calls
 */
static int
config_amd762(int enable)
{
	unsigned int regdword;

	/* Enable STPGNT in BIU Status/Control for cpu0 */
	pci_read_config_dword(pdev_nb, 0x60, &regdword);
	regdword |= (1 << 17);
	pci_write_config_dword(pdev_nb, 0x60, regdword);

	/* Enable STPGNT in BIU Status/Control for cpu1 */
	pci_read_config_dword(pdev_nb, 0x68, &regdword);
	regdword |= (1 << 17);
	pci_write_config_dword(pdev_nb, 0x68, regdword);

	/* DRAM refresh enable */
	pci_read_config_dword(pdev_nb, 0x58, &regdword);
	regdword &= ~(1 << 19);
	pci_write_config_dword(pdev_nb, 0x58, regdword);

	/* Self refresh enable */
	pci_read_config_dword(pdev_nb, 0x70, &regdword);
	regdword |= (1 << 18);
	pci_write_config_dword(pdev_nb, 0x70, regdword);

	return 0;
}


/*
 * Get the base PMIO address and set the pm registers in amd76x_pm_cfg.
 */
static void
amd76x_get_PM(void)
{
	unsigned int regdword;

	/* Get the address for pm status, P_LVL2, etc */
	pci_read_config_dword(pdev_sb, 0x58, &regdword);
	regdword &= 0xff80;
	amd76x_pm_cfg.status_reg = (regdword + 0x00);
	amd76x_pm_cfg.slp_reg =    (regdword + 0x04);
	amd76x_pm_cfg.NTH_reg =    (regdword + 0x10);
	amd76x_pm_cfg.C2_reg =     (regdword + 0x14);
	amd76x_pm_cfg.C3_reg =     (regdword + 0x15);
	amd76x_pm_cfg.resume_reg = (regdword + 0x16); /* N/A for 768 */
}


/*
 * En/Disable PMIO and configure W4SG & STPGNT.
 */
static int
config_PMIO_amd76x(int is_766, int enable)
{
	unsigned char regbyte;

	/* Clear W4SG, and set PMIOEN, if using a 765/766 set STPGNT as well.
	 * AMD-766: C3A41; page 59 in AMD-766 doc
	 * AMD-768: DevB:3x41C; page 94 in AMD-768 doc */
	pci_read_config_byte(pdev_sb, 0x41, &regbyte);
	if(enable) {
		regbyte |= ((0 << 0) | (is_766?1:0 << 1) | (1 << 7));
	}
	else {
		regbyte |= (0 << 7);
	}
	pci_write_config_byte(pdev_sb, 0x41, regbyte);

	return 0;
}

/*
 * C2 idle support for AMD-766.
 */
static void
config_amd766_C2(int enable)
{
	unsigned int regdword;

	/* Set C2 options in C3A50, page 63 in AMD-766 doc */
	pci_read_config_dword(pdev_sb, 0x50, &regdword);
	if(enable) {
		regdword &= ~((DCSTOP_EN | CPUSTP_EN | PCISTP_EN | SUSPND_EN |
					CPURST_EN) << C2_REGS);
		regdword |= (STPCLK_EN	/* ~ 20 Watt savings max */
			 |  CPUSLP_EN)	/* Additional ~ 70 Watts max! */
			 << C2_REGS;
	}
	else
		regdword &= ~((STPCLK_EN | CPUSLP_EN) << C2_REGS);
	pci_write_config_dword(pdev_sb, 0x50, regdword);
}


#ifdef AMD76X_C3
/*
 * Untested C3 idle support for AMD-766.
 */
static void
config_amd766_C3(int enable)
{
	unsigned int regdword;

	/* Set C3 options in C3A50, page 63 in AMD-766 doc */
	pci_read_config_dword(pdev_sb, 0x50, &regdword);
	if(enable) {
		regdword &= ~((DCSTOP_EN | PCISTP_EN | SUSPND_EN | CPURST_EN)
				<< C3_REGS);
		regdword |= (STPCLK_EN	/* ~ 20 Watt savings max */
			 |  CPUSLP_EN	/* Additional ~ 70 Watts max! */
			 |  CPUSTP_EN)	/* yet more savings! */
			 << C3_REGS;
	}
	else
		regdword &= ~((STPCLK_EN | CPUSLP_EN | CPUSTP_EN) << C3_REGS);
	pci_write_config_dword(pdev_sb, 0x50, regdword);
}
#endif


#ifdef AMD76X_POS
static void
config_amd766_POS(int enable)
{
	unsigned int regdword;

	/* Set C3 options in C3A50, page 63 in AMD-766 doc */
	pci_read_config_dword(pdev_sb, 0x50, &regdword);
	if(enable) {
		regdword &= ~((ZZ_CACHE_EN | CPURST_EN) << POS_REGS);
		regdword |= ((DCSTOP_EN | STPCLK_EN | CPUSTP_EN | PCISTP_EN |
					CPUSLP_EN | SUSPND_EN) << POS_REGS);
	}
	else
		regdword ^= (0xff << POS_REGS);
	pci_write_config_dword(pdev_sb, 0x50, regdword);
}
#endif


/*
 * Configures the 765 & 766 southbridges.
 */
static int
config_amd766(int enable)
{
	amd76x_get_PM();
	config_PMIO_amd76x(1, 1);

	config_amd766_C2(enable);
#ifdef AMD76X_C3
	config_amd766_C3(enable);
#endif
#ifdef AMD76X_POS
	config_amd766_POS(enable);
#endif

	return 0;
}


/*
 * C2 idling support for AMD-768.
 */
static void
config_amd768_C2(int enable)
{
	unsigned char regbyte;
	
	/* Set C2 options in DevB:3x4F, page 100 in AMD-768 doc */
	pci_read_config_byte(pdev_sb, 0x4F, &regbyte);
	if(enable)
		regbyte |= C2EN;
	else
		regbyte ^= C2EN;
	pci_write_config_byte(pdev_sb, 0x4F, regbyte);
}


#ifdef AMD76X_C3
/*
 * C3 idle support for AMD-768. The idle loop would need some extra
 * handling for C3, but it would make more sense for ACPI to handle CX level
 * transitions like it is supposed to. Unfortunately ACPI doesn't do CX
 * levels on SMP systems yet.
 */
static void
config_amd768_C3(int enable)
{
	unsigned char regbyte;
	
	/* Set C3 options in DevB:3x4F, page 100 in AMD-768 doc */
	pci_read_config_byte(pdev_sb, 0x4F, &regbyte);
	if(enable)
		regbyte |= (C3EN /* | ZZ_C3EN | CSLP_C3EN | CSTP_C3EN */);
	else
		regbyte ^= C3EN;
	pci_write_config_byte(pdev_sb, 0x4F, regbyte);
}
#endif


#ifdef AMD76X_POS
/*
 * Untested Power On Suspend support for AMD-768. This should also be handled
 * by ACPI.
 */
static void
config_amd768_POS(int enable)
{
	unsigned int regdword;

	/* Set POS options in DevB:3x50, page 101 in AMD-768 doc */
	pci_read_config_dword(pdev_sb, 0x50, &regdword);
	if(enable) 
		regdword |= (POSEN | CSTP | PSTP | ASTP | DCSTP | CSLP | SUSP);
	else
		regdword ^= POSEN;
	pci_write_config_dword(pdev_sb, 0x50, regdword);
}
#endif


#ifdef AMD76X_NTH
/*
 * Normal Throttling support for AMD-768. There are several settings
 * that can be set depending on how long you want some of the delays to be.
 * I'm not sure if this is even neccessary at all as the 766 doesn't need this.
 */
static void
config_amd768_NTH(int enable, int ntper, int thminen)
{
	unsigned char regbyte;

	/* DevB:3x40, pg 93 of 768 doc */
	pci_read_config_byte(pdev_sb, 0x40, &regbyte);
	/* Is it neccessary to use THMINEN at ANY time? */
	regbyte |= (NTPER(ntper) | THMINEN(thminen));
	pci_write_config_byte(pdev_sb, 0x40, regbyte);
}
#endif


/*
 * Configures the 768 southbridge to support idle calls, and gets
 * the processor idle call register location.
 */
static int
config_amd768(int enable)
{
	amd76x_get_PM();
	config_PMIO_amd76x(0, 1);

	config_amd768_C2(enable);
#ifdef AMD76X_C3
	config_amd768_C3(enable);
#endif
#ifdef AMD76X_POS
	config_amd768_POS(enable);
#endif
#ifdef AMD76X_NTH
	config_amd768_NTH(enable, 1, 2);
#endif

	return 0;
}


#ifdef AMD76X_NTH
/*
 * Activate normal throttling via its ACPI register (P_CNT).
 */
static void
activate_amd76x_NTH(int enable, int ratio)
{
	unsigned int regdword;

	/* PM10, pg 110 of 768 doc, pg 70 of 766 doc */
	regdword=inl(amd76x_pm_cfg.NTH_reg);
	if(enable)
		regdword |= (NTH_EN | NTH_RATIO(ratio));
	else
		regdword ^= NTH_EN;
	outl(regdword, amd76x_pm_cfg.NTH_reg);
}
#endif

#ifdef AMD76X_POS
/*
 * Activate sleep state via its ACPI register (PM1_CNT).
 */
static void
activate_amd76x_SLP(int type)
{
	unsigned short regshort;

	/* PM04, pg 109 of 768 doc, pg 69 of 766 doc */
	regshort=inw(amd76x_pm_cfg.slp_reg);
	regshort |= (SLP_EN | SLP_TYP(type)) ;
	outw(regshort, amd76x_pm_cfg.slp_reg);
}

/*
 * Wrapper function to activate POS sleep state.
 */
static void
activate_amd76x_POS(void)
{
	activate_amd76x_SLP(1);
}
#endif


#if 0
/*
 * Idle loop for single processor systems
 */
void
amd76x_up_idle(void)
{
	// FIXME: Optionally add non-smp idle loop here
}
#endif


/*
 * Idle loop for SMP systems, supports currently only 2 processors.
 *
 * Note; for 2.5 folks - not pre-empt safe
 */
static void
amd76x_smp_idle(void)
{

	/*
	 * Exit idle mode immediately if the CPU does not change.
	 * Usually that means that we have some load on another CPU.
	 */
	if (prs[0].idle && prs[1].idle && amd76x_pm_cfg.last_pr == smp_processor_id()) {
		prs[0].idle = 0;
		prs[1].idle = 0;
		/* This looks redundent as it was just checked in the if() */
		/* amd76x_pm_cfg.last_pr = smp_processor_id(); */
		return;
	}

	prs[smp_processor_id()].count++;

	/* Don't start the idle mode immediately */
	if (prs[smp_processor_id()].count >= LAZY_IDLE_DELAY) {

		/* Put the current processor into idle mode */
		prs[smp_processor_id()].idle =
			(prs[smp_processor_id()].idle ? 2 : 1);

		/* Only idle if both processors are idle */
		if ((prs[0].idle==1) && (prs[1].idle==1)) {
			amd76x_pm_cfg.C2_cnt++;
			inb(amd76x_pm_cfg.C2_reg);
		}
	#ifdef AMD76X_C3
		/*
		 * JH: I've not been able to get into here. Could this have
		 * something to do with the way the kernel handles the idle
		 * loop, or and error that I've made?
		 */
		else if ((prs[0].idle==2) && (prs[1].idle==2)) {
			amd76x_pm_cfg.C3_cnt++;
			inb(amd76x_pm_cfg.C3_reg);
		}
	#endif

		prs[smp_processor_id()].count = 0;

	}
	amd76x_pm_cfg.last_pr = smp_processor_id();
}


/*
 * Finds and initializes the bridges, and then sets the idle function
 */
static int
amd76x_pm_main(void)
{
	int found;

	/* Find northbridge */
	found = pci_register_driver(&amd_nb_driver);
	if (found <= 0) {
		printk(KERN_ERR "amd76x_pm: Could not find northbridge\n");
		pci_unregister_driver(&amd_nb_driver);
		return 1;
	}

	/* Find southbridge */
	found = pci_register_driver(&amd_sb_driver);
	if (found <= 0) {
		printk(KERN_ERR "amd76x_pm: Could not find southbridge\n");
		pci_unregister_driver(&amd_sb_driver);
		pci_unregister_driver(&amd_nb_driver);
		return 1;
	}

	/* Init southbridge */
	switch (pdev_sb->device) {
	case PCI_DEVICE_ID_AMD_VIPER_7413:	/* AMD-765 or 766 */
		config_amd766(1);
		break;
	case PCI_DEVICE_ID_AMD_VIPER_7443:	/* AMD-768 */
		config_amd768(1);
		break;
	default:
		printk(KERN_ERR "amd76x_pm: No southbridge to initialize\n");
		break;
	}

	/* Init northbridge and queue the new idle function */
	switch (pdev_nb->device) {
	case PCI_DEVICE_ID_AMD_FE_GATE_700C:	/* AMD-762 */
		config_amd762(1);
#ifndef AMD76X_NTH
		amd76x_pm_cfg.curr_idle = amd76x_smp_idle;
#endif
		break;
	default:
		printk(KERN_ERR "amd76x_pm: No northbridge to initialize\n");
		break;
	}

#ifndef AMD76X_NTH
	if (!amd76x_pm_cfg.curr_idle) {
		printk(KERN_ERR "amd76x_pm: Idle function not changed\n");
		pci_unregister_driver(&amd_nb_driver);
		pci_unregister_driver(&amd_sb_driver);
		return 1;
	}

	amd76x_pm_cfg.orig_idle = pm_idle;
	pm_idle = amd76x_pm_cfg.curr_idle;
#endif

#ifdef AMD76X_NTH
	/* Turn NTH on with maxium throttling for testing. */
	activate_amd76x_NTH(1, 1);
#endif

#ifdef AMD76X_POS
	/* Testing here only. */
	activate_amd76x_POS();
#endif

	return 0;
}


static int __init
amd76x_pm_init(void)
{
	printk(KERN_INFO "amd76x_pm: Version %s\n", VERSION);
	return amd76x_pm_main();
}


static void __exit
amd76x_pm_cleanup(void)
{
#ifndef AMD76X_NTH
	pm_idle = amd76x_pm_cfg.orig_idle;

	/* This isn't really needed. */
	printk(KERN_INFO "amd76x_pm: %lu C2 calls\n", amd76x_pm_cfg.C2_cnt);
#ifdef AMD76X_C3
	printk(KERN_INFO "amd76x_pm: %lu C3 calls\n", amd76x_pm_cfg.C3_cnt);
#endif

	/* 
	 * FIXME: We want to wait until all CPUs have set the new
	 * idle function, otherwise we will oops. This may not be
	 * the right way to do it, but seems to work.
	 *
	 * - Best answer is going to be to ban unload, but when its debugged
	 *   --- Alan
	 */
	schedule();
	mdelay(1000);
#endif

#ifdef AMD76X_NTH
	/* Turn NTH off*/
	activate_amd76x_NTH(0, 0);
#endif

	pci_unregister_driver(&amd_nb_driver);
	pci_unregister_driver(&amd_sb_driver);

}


MODULE_LICENSE("GPL");
module_init(amd76x_pm_init);
module_exit(amd76x_pm_cleanup);
