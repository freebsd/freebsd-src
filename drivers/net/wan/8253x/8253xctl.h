/* -*- linux-c -*- */
/* 
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

/* These structures and symbols are less related to the ESCC2s and ESCC8s per se and more to */
/* the architecture of Aurora cards or to the logic of the driver */

#ifndef _ATICNTRL_H_
#define _ATICNTRL_H_

#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/autoconf.h>
#include <asm/page.h>
#include "Reg9050.h"
#include "8253x.h"

#define FALSE	0
#define TRUE	1

#define NUMINTS 32

/* The following are suggested by serial.h -- all not currently used */
#define FLAG8253X_HUP_NOTIFY	0x0001 /* Notify getty on hangups and closes 
					  on the callout port */ 
#define FLAG8253X_FOURPORT	0x0002 /* Set OU1, OUT2 per AST Fourport settings */
#define FLAG8253X_SAK		0x0004 /* Secure Attention Key (Orange book) */
#define FLAG8253X_SPLIT_TERMIOS 0x0008 /* Separate termios for dialin/callout */
#define FLAG8253X_SPD_MASK	0x1030
#define FLAG8253X_SPD_HI	0x0010	/* Use 56000 instead of 38400 bps */
#define FLAG8253X_SPD_VHI	0x0020  /* Use 115200 instead of 38400 bps */
#define FLAG8253X_SPD_CUST	0x0030  /* Use user-specified divisor */
#define FLAG8253X_SKIP_TEST	0x0040 /* Skip UART test during autoconfiguration */
#define FLAG8253X_AUTO_IRQ	0x0080 /* Do automatic IRQ during autoconfiguration */

#define FLAG8253X_SESSION_LOCKOUT 0x0100 /* Lock out cua opens based on session */
#define FLAG8253X_PGRP_LOCKOUT    0x0200 /* Lock out cua opens based on pgrp */
#define FLAG8253X_CALLOUT_NOHUP   0x0400 /* Don't do hangups for cua device */

#define FLAG8253X_HARDPPS_CD	0x0800	/* Call hardpps when CD goes high  */
#define FLAG8253X_SPD_SHI	0x1000	/* Use 230400 instead of 38400 bps */
#define FLAG8253X_SPD_WARP	0x1010	/* Use 460800 instead of 38400 bps */

#define FLAG8253X_LOW_LATENCY	0x2000 /* Request low latency behaviour */

#define FLAG8253X_BUGGY_UART	0x4000 /* This is a buggy UART, skip some safety
					* checks.  Note: can be dangerous! */

#define FLAG8253X_AUTOPROBE	0x8000 /* Port was autoprobed by PCI or PNP code */

#define FLAG8253X_FLAGS		0x7FFF	/* Possible legal async flags */
#define FLAG8253X_USR_MASK	0x3430	/* Legal flags that non-privileged
					 * users can set or reset */

/* Internal flags used only by the 8253x driver */
#define FLAG8253X_INITIALIZED	0x80000000 /* Serial port was initialized */
#define FLAG8253X_CALLOUT_ACTIVE 0x40000000 /* Call out device is active */
#define FLAG8253X_NORMAL_ACTIVE	0x20000000 /* Normal device is active */
#define FLAG8253X_BOOT_AUTOCONF	0x10000000 /* Autoconfigure port on bootup */
#define FLAG8253X_CLOSING	0x08000000 /* Serial port is closing */
#define FLAG8253X_CTS_FLOW	0x04000000 /* Do CTS flow control */
#define FLAG8253X_CHECK_CD	0x02000000 /* i.e., CLOCAL */
#define FLAG8253X_NETWORK	0x01000000 /* the logic of callout
					    * and reconnect works differently
					    * for network ports*/

#define FLAG8253X_CONS_FLOW	0x00800000 /* flow control for console  */

#define FLAG8253X_INTERNAL_FLAGS 0xFF800000 /* Internal flags */

#define SAB8253X_CLOSING_WAIT_INF	0
#define SAB8253X_CLOSING_WAIT_NONE	65535

#define SAB8253X_EVENT_WRITE_WAKEUP 0

typedef struct AuraXX20params
{
	unsigned debug;		/* lots of kernel warnings */
	unsigned listsize;		/* size of descriptor list */
} AURAXX20PARAMS;

/* initialization functions */
extern unsigned int
plx9050_eprom_read(unsigned int* eprom_ctl, unsigned short *ptr, unsigned char addr, unsigned short len);
extern unsigned int 
plx9050_eprom_cmd(unsigned int* eprom_ctl, unsigned char cmd, unsigned char addr, unsigned short data);
extern void dump_ati_adapter_registers(unsigned int *addr, int len);

/* common routine */
extern void sab8253x_interrupt(int irq, void *dev_id, struct pt_regs *regs);

/* net device functions */
extern int Sab8253xInitDescriptors2(SAB_PORT *priv, int listsize, int rbufsize);
extern int sab8253xn_init(struct net_device *dev); /* called by registration */
extern int sab8253xn_write2(struct sk_buff *skb, struct net_device *dev); 
				/* hard_start_xmit */
extern int sab8253xn_ioctl(struct net_device *dev, struct ifreq *ifr,
			   int cmd); 
				/* interrupt handler */
extern void sab8253xn_handler2(int irq, void *devidp, struct pt_regs* ptregsp);
extern int sab8253xn_open(struct net_device *dev);
extern int sab8253xn_release(struct net_device *dev);	/* stop */
extern struct net_device_stats *sab8253xn_stats(struct net_device *dev);

extern struct net_device *Sab8253xRoot;
extern struct net_device auraXX20n_prototype;
extern SAB_PORT *current_sab_port;
extern int sab8253xn_listsize;
extern int sab8253xn_rbufsize;
extern int sab8253xt_listsize;
extern int sab8253xt_rbufsize;
extern int sab8253xs_listsize;
extern int sab8253xs_rbufsize;
extern int sab8253xc_listsize;
extern int sab8253xc_rbufsize;

/* character device functions */

extern int 
sab8253xc_read(struct file *filep, char *cptr, size_t cnt, loff_t *loffp);
extern int sab8253xc_write(struct file *filep, const char *cptr, size_t cnt, loff_t *loffp);
extern int sab8253xc_open(struct inode *inodep, struct file *filep);
extern int sab8253xc_release(struct inode *inodep, struct file *filep);
extern unsigned int sab8253xc_poll(struct file *, struct poll_table_struct *);
extern int sab8253xc_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
extern int sab8253xc_fasync(int, struct file *, int);

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

#define SERIAL_PARANOIA_CHECK
#define SERIAL_DO_RESTART

/* sync tty functions */

/* general macros */
#define DEBUGPRINT(arg) if(DRIVER_DEBUG()) printk arg
#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))

extern AURAXX20PARAMS AuraXX20DriverParams;
#define DRIVER_DEBUG() (AuraXX20DriverParams.debug)
#define DRIVER_LISTSIZE() (AuraXX20DriverParams.listsize)
#define XSETDRIVER_LISTSIZE(arg) (AuraXX20DriverParams.listsize = (arg))

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#define	net_device_stats enet_statistics
#define net_device device
#define pci_base_address(p, n)  (p)->base_address[n]
#define dev_kfree_skb_irq(s) dev_kfree_skb((s))
#define dev_kfree_skb_any(s) dev_kfree_skb((s))
#else  /* LINUX_VERSION_CODE */
#define NETSTATS_VER2
#define pci_base_address(p, n) pci_resource_start((p), (n))
#endif /* LINUX_VERSION_CODE */

				/* The sample LINUX driver */
				/* uses these no reason not */
				/* to be more dynamic.*/

/* Below are things that should be placed in <linux/pci_ids.h> */
#ifndef PCI_VENDOR_ID_AURORATECH
#define PCI_VENDOR_ID_AURORATECH 0x125c
                                  /* Saturn and Apollo boards */ 
#define PCI_DEVICE_ID_AURORATECH_MULTI   0x0101 
                                  /* WAN/LAN Multiservers */
#define PCI_DEVICE_ID_AURORATECH_WANMS   0x0102  
                                  /* Comnpact PCI boards */
#define PCI_DEVICE_ID_AURORATECH_CPCI    0x0103  
#endif /* PCI_VENDOR_ID_AURORATECH */

extern int sab8253x_vendor_id;
extern int sab8253x_cpci_device_id;
extern int sab8253x_wmcs_device_id;
extern int sab8253x_mpac_device_id;

#define PCIMEMVALIDMULTI	((sab8253x_vendor_id << 16) | sab8253x_mpac_device_id)
#define PCIMEMVALIDCPCI		((sab8253x_vendor_id << 16) | sab8253x_cpci_device_id)
#define PCIMEMVALIDWMCS		(sab8253x_vendor_id | (sab8253x_wmcs_device_id << 16))

/* 
 * Some values defining boards
 *
 * First 1, 2, 4 and 8 ports Multiports
 */

#define AURORA_8X20_CHIP_NPORTS      8
#define AURORA_4X20_CHIP_NPORTS      2 /* uses two ESCC2s */
#define AURORA_2X20_CHIP_NPORTS      2
#define AURORA_1X20_CHIP_NPORTS      1

				/* sizes of PLX9050 address space */
#define AURORA_4X20_SIZE		0x800 
#define AURORA_4X20_CHIP_OFFSET           0x400
#define AURORA_8X20_SIZE		0x200
#define AURORA_8X20_CHIP_OFFSET           0x0
#define AURORA_2X20_SIZE		0x80 /* This looks wrong probably las0 size */
#define AURORA_2X20_CHIP_OFFSET           0x0

#define AURORA_MULTI_1PORTBIT		PLX_CTRL_USERIO3DATA
#define AURORA_MULTI_SYNCBIT		PLX_CTRL_USERIO3DIR

#define AURORA_MULTI_CLKSPEED	((unsigned long) 29491200) /* 29.4912 MHz */
#define SUN_SE_CLKSPEED	        ((unsigned long) 29491200) /* 29.4912 MHz */

/*
 * Per-CIM structure
 */

#define CIM_SEPLEN	128	/* serial EPROM length on a CIM */
#define CIM_REVLEN	17
#define CIM_SNLEN	17
#define CIM_MFGLOCLEN	17
#define CIM_MFGDATELEN	33

/* CIM types: */
#define CIM_UNKNOWN	0
#define CIM_RS232	1	/* RS-232 only CIM */
#define CIM_SP502	2	/* SP502 multi-mode CIM */

/* CIM flags: */
#define CIM_SYNC	0x0000001	/* sync allowed */
#define CIM_PROTOTYPE	0x0000002	/* prototype */
#define CIM_LAST	0x0000100	/* the last CIM */

typedef struct aura_cim 
{
	int			ci_type;	/* CIM type */
	unsigned long	 	ci_flags;	/* CIM flags */
	int			ci_num;	/* the canonical number of this CIM */
	int			ci_port0lbl;	/* what's the label on port 0 */
	unsigned int	 	ci_nports;	/* the number of ports on this CIM */
	struct sab_board	*ci_board;	/* pointer back to the board */
	struct sab_chip		*ci_chipbase;	/* first chip */
	struct sab_port		*ci_portbase;	/* first port */
	unsigned long	 	ci_clkspeed;	/* the clock speed */
	unsigned int		ci_clkspdsrc;
	int			ci_spdgrd;	/* chip speed grade */
	unsigned int		ci_spdgrdsrc;
	unsigned char	 	ci_sep[CIM_SEPLEN];
	unsigned char	 	ci_rev[CIM_REVLEN];	/* revision information */
	unsigned char	 	ci_sn[CIM_SNLEN];	/* serial number */
	unsigned char	 	ci_mfgdate[CIM_MFGDATELEN];
	unsigned char	 	ci_mfgloc[CIM_MFGLOCLEN];
	struct aura_cim		*next;
	struct aura_cim		*next_by_mcs;
} aura_cim_t, AURA_CIM;

/*
 * Board structure
 */

typedef struct sab_board 
{
	struct sab_board 	*nextboard;
	struct sab_board	*next_on_interrupt;
	
	struct sab_chip		*board_chipbase;	/* chip list for this board */
	struct sab_port		*board_portbase;	/* port list for this board */
	unsigned int		board_number;
	
#define BD_1020P	1
#define BD_1520P	2
#define BD_2020P	3
#define BD_2520P	4
#define BD_4020P	5
#define BD_4520P	6
#define BD_8020P	7
#define BD_8520P	8
#define BD_SUNSE	9	/* Sun's built-in 82532 -- not supported by this driver*/
#define BD_WANMCS	10	
#define BD_1020CP	11	/* CPCI start here */
#define BD_1520CP	12
#define BD_2020CP	13
#define BD_2520CP	14
#define BD_4020CP	15
#define BD_4520CP	16
#define BD_8020CP	17
#define BD_8520CP	18
#define BD_ISCPCI(x)	((x) == BD_1020CP || (x) == BD_1520CP		\
			 || (x) == BD_2020CP || (x) == BD_2520CP	\
			 || (x) == BD_4020CP || (x) == BD_4520CP	\
			 || (x) == BD_8020CP || (x) == BD_8520CP)

	unsigned char		b_type;
	unsigned char		b_nchips;	/* num chips for this board */
	unsigned char		b_nports;	/* num ports for this board */
	unsigned char		b_flags;	/* flags: */
#define BD_SYNC		0x01	/* sync is allowed on this board */
#define BD_INTRHI	0x02	/* intr is hi level (SPARC only) */
	struct pci_dev      	b_dev;         /* what does PCI knows about us */
	int                  	b_irq;           /* For faster access */
	unsigned char		*b_chipregbase;	/* chip register io */
	long		 	b_clkspeed;	/* the clock speed */
	int			b_spdgrd;	/* chip speed grade */
	unsigned short		b_intrmask;	/* wanmcs interrupt mask */
	unsigned char*		virtbaseaddress0;
	unsigned int		length0;
	unsigned char*		virtbaseaddress1;
	unsigned int		length1;
	unsigned char*		virtbaseaddress2;
	unsigned int		length2;
	unsigned char*		virtbaseaddress3;
	unsigned int		length3;
  
  /*
   * Pointer to hardware-specific electrical interface code.  This
   *  routine will set the electrical interface to whatever is in
   *  the given interface/txena pair.
   * Returns 0 if it could not set the value, non-zero otherwise.
   */
	int			(*b_setif)(struct sab_port *line,
					   int interface, int txena);
	/*
	 * hardware mapping
	 */
	unsigned int	 	b_eprom[64];	/* serial EPROM contents */
	AURA_CIM		*b_cimbase;	/* CIM information */
	PLX9050		*b_bridge;	/* PCI bridge ctlr. regs. */
} sab_board_t, SAB_BOARD;	/* really hate the _t convention */

#define SEPROM 1		/* driver got clock speed from SEPROM */

#define SAB8253X_XMIT_SIZE PAGE_SIZE

extern char *aura_functionality[];

#endif /* _ATICNTRL_H_ */

