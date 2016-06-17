/* $Id: ptifddi.h,v 1.3 1999/08/20 00:31:08 davem Exp $
 * ptifddi.c: Defines for Performance Technologies FDDI sbus cards.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _PTIFDDI_H
#define _PTIFDDI_H

struct dpram_loader {
	volatile unsigned char dpram_stat;
	volatile unsigned char _unused;
	volatile unsigned char addr_low;
	volatile unsigned char addr_hi;
	volatile unsigned char num_bytes;
	volatile unsigned char data[0x3b];

	volatile unsigned char loader_firmware[0xc0];
};

struct dfddi_ram {
/*0x000*/	unsigned char		_unused0[0x100];
/*0x100*/	struct dpram_loader	loader;
/*0x200*/	unsigned char		instructions[0x400];
/*0x600*/	unsigned char		msg_in[0x20];
/*0x620*/	unsigned char		msg_out[0x20];
/*0x640*/	unsigned char		_unused2[0x50];
/*0x690*/	unsigned char		smsg_in[0x20];
/*0x6b0*/	unsigned char		_unused3[0x30];
/*0x6e0*/	unsigned char		beacom_frame[0x20];
/*0x700*/	unsigned char		re_sync;
/*0x701*/	unsigned char		_unused4;
/*0x702*/	unsigned short		tswitch;
/*0x704*/	unsigned char		evq_lost;
/*0x705*/	unsigned char		_unused6;
/*0x706*/	unsigned char		signal_lost;
/*0x707*/	unsigned char		_unused7;
/*0x708*/	unsigned char		lerror;
/*0x709*/	unsigned char		_unused8;
/*0x70a*/	unsigned char		rstate;
/*0x70b*/	unsigned char		_unused9[0x13];
/*0x716*/	unsigned short		dswitch;
/*0x718*/	unsigned char		_unused10[0x48];
/*0x750*/	unsigned char		cbusy;
/*0x751*/	unsigned char		hbusy;
/*0x752*/	unsigned short		istat;
/*0x754*/	unsigned char		_unused11[];
/*0x756*/	unsigned char		disable;
/*0x757*/	unsigned char		_unused12[];
/*0x78e*/	unsigned char		ucvalid;
/*0x78f*/	unsigned char		_unused13;
/*0x790*/	unsigned int		u0addr;
/*0x794*/	unsigned char		_unused14[];
/*0x7a8*/	unsigned int		P_player;
/*0x7ac*/	unsigned int		Q_player;
/*0x7b0*/	unsigned int		macsi;
/*0x7b4*/	unsigned char		_unused15[];
/*0x7be*/	unsigned short		reset;
/*0x7c0*/	unsigned char		_unused16[];
/*0x7fc*/	unsigned short		iack;
/*0x7fe*/	unsigned short		loader_addr;
};

#define DPRAM_SIZE		0x800

#define DPRAM_STAT_VALID	0x80
#define DPRAM_STAT_EMPTY	0x00

struct ptifddi {
	struct dfddi_ram	*dpram;
	unsigned char		*reset;
	unsigned char		*unreset;
	struct net_device		*dev;
	struct ptifddi		*next_module;
};

#endif /* !(_PTIFDDI_H) */
