/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "DIALOUT"
 */

#line 1 "../../i386/conf/ioconf.c.i386"
/*-
 * Copyright (c) 1994, 1995, 1996 Berkeley Software Design, Inc.
 * All rights reserved.
 * The Berkeley Software Design Inc. software License Agreement specifies
 * the terms and conditions for redistribution.
 *
 *	BSDI $Id$
 *
 * WILDBOAR $Wildboar: ioconf.c.i386,v 1.8 1996/02/13 13:01:15 shigeya Exp $
 *
 *  Portions or all of this file are Copyright(c) 1994,1995,1996
 *  Yoichi Shinoda, Yoshitaka Tokugawa, WIDE Project, Wildboar Project
 *  and Foretune.  All rights reserved.
 *
 *  This code has been contributed to Berkeley Software Design, Inc.
 *  by the Wildboar Project and its contributors.
 */

/* template ioconf.c for i386 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <i386/isa/isa.h>
#include <i386/isa/isavar.h>
#include <i386/isa/icu.h>

extern struct cfdriver tgcd;
extern struct cfdriver sdcd;
extern struct cfdriver srcd;
extern struct cfdriver stcd;
extern struct cfdriver sgcd;
extern struct cfdriver isacd;
extern struct cfdriver pcconscd;
extern struct cfdriver pcauxcd;
extern struct cfdriver comcd;
extern struct cfdriver lpcd;
extern struct cfdriver fdccd;
extern struct cfdriver fdcd;
extern struct cfdriver dptcd;
extern struct cfdriver wdccd;
extern struct cfdriver wdcd;
extern struct cfdriver wdpicd;
extern struct cfdriver mcdcd;
extern struct cfdriver wtcd;
extern struct cfdriver npxcd;
extern struct cfdriver vgacd;
extern struct cfdriver bmscd;
extern struct cfdriver lmscd;
extern struct cfdriver ahacd;
extern struct cfdriver bhacd;
extern struct cfdriver necd;
extern struct cfdriver epcd;
extern struct cfdriver sacd;
extern struct cfdriver ncrcd;
extern struct cfdriver saturncd;
extern struct cfdriver aiccd;
extern struct cfdriver tncd;
extern struct cfdriver hppcd;
extern struct cfdriver recd;
extern struct cfdriver wecd;
extern struct cfdriver tlcd;
extern struct cfdriver ebcd;
extern struct cfdriver efcd;
extern struct cfdriver elcd;
extern struct cfdriver excd;
extern struct cfdriver eahacd;
extern struct cfdriver pciccd;
extern struct cfdriver ccecd;
extern struct cfdriver mzcd;
extern struct cfdriver decd;
extern struct cfdriver expcd;
extern struct cfdriver nsphycd;
extern struct cfdriver ics90pcd;
extern struct cfdriver i555pcd;
extern struct cfdriver tn100acd;


/* locators */
static int loc[395] = {
	0, 0, 0, 0, -1, -1, BUS_EISA, 0,
	0, 0, 0, -1, -1, BUS_PCI, IO_KBD, 0,
	0, 0, -1, -1, BUS_ISA, IO_KBD, 0, 0,
	0, 0xc, -1, BUS_ISA, 0x3e0, 0, 0, 0,
	0xb, -1, BUS_ISA, 0x3e2, 0, 0, 0, 0xa,
	-1, BUS_ISA, 0x3e4, 0, 0, 0, 0xa, -1,
	BUS_ISA, IO_COM1, 0, 0, 0, -1, -1, BUS_ISA,
	IO_COM2, 0, 0, 0, -1, -1, BUS_ISA, 0,
	0, 0, 0, -1, -1, BUS_PCMCIA, 0x378, 0,
	0, 0, 7, -1, BUS_ISA, 0x3bc, 0, 0,
	0, 7, -1, BUS_ISA, IO_FD1, 0, 0, 0,
	-1, 2, BUS_ISA, 0x280, 0, 0, 0, -1,
	-1, BUS_ISA, 0x2a0, 0, 0, 0, -1, -1,
	BUS_ISA, 0x2e0, 0, 0, 0, -1, -1, BUS_ISA,
	0x300, 0, 0, 0, -1, -1, BUS_ISA, 0x310,
	0, 0, 0, -1, -1, BUS_ISA, 0x330, 0,
	0, 0, -1, -1, BUS_ISA, 0x350, 0, 0,
	0, -1, -1, BUS_ISA, 0x250, 0, 0, 0,
	-1, -1, BUS_ISA, 0x260, 0, 0, 0, -1,
	-1, BUS_ISA, 0x310, 0, 0xd0000, 0x10000, -1, -1,
	BUS_ISA, 0, 0, 0, 0, -1, -1, BUS_ANY,
	0x170, 0, 0, 0, -1, 5, BUS_ISA, IO_WD1,
	0, 0, 0, -1, -1, BUS_ISA, IO_WD2, 0,
	0, 0, -1, -1, BUS_ISA, 0x300, 0, 0,
	0, -1, 1, BUS_ISA, IO_NPX, 0, 0, 0,
	-1, -1, BUS_ISA, IO_VGA, 0, 0xa0000, 0x10000, -1,
	-1, BUS_ISA, 0x23c, 0, 0, 0, 5, -1,
	BUS_ISA, 0x334, 0, 0, 0, 9, -1, BUS_ISA,
	0x340, 0, 0, 0, 9, -1, BUS_ISA, 0x334,
	0, 0, 0, -1, -1, BUS_ISA, 0x340, 0,
	0, 0, -1, -1, BUS_ISA, 0x320, 0, 0,
	0, -1, -1, BUS_ISA, 0x360, 0, 0, 0,
	-1, -1, BUS_ISA, 0x240, 0, 0, 0, -1,
	-1, BUS_ISA, 0x320, 0, 0, 0, -1, 3,
	BUS_ISA, 0x340, 0, 0, 0, -1, 3, BUS_ISA,
	0x360, 0, 0, 0, -1, 3, BUS_ISA, 0x300,
	0, 0, 0, -1, 3, BUS_ISA, 0x2c0, 0,
	0xc8000, 0x800, -1, -1, BUS_ISA, 0x300, 0, 0xc8000,
	0x800, -1, -1, BUS_ISA, 0x380, 0, 0, 0,
	-1, -1, BUS_ISA, 0x280, 0, 0xd0000, 0x4000, -1,
	-1, BUS_ISA, 0x2a0, 0, 0xd0000, 0x4000, -1, -1,
	BUS_ISA, 0x2c0, 0, 0xd0000, 0x4000, -1, -1, BUS_ISA,
	0x2e0, 0, 0xd0000, 0x4000, -1, -1, BUS_ISA, 0x300,
	0, 0xd0000, 0x4000, -1, -1, BUS_ISA, 0x320, 0,
	0xd0000, 0x4000, -1, -1, BUS_ISA, 0x340, 0, 0xd0000,
	0x4000, -1, -1, BUS_ISA, 0x360, 0, 0xd0000, 0x4000,
	-1, -1, BUS_ISA, 0x380, 0, 0xd0000, 0x4000, -1,
	-1, BUS_ISA, 0x3a0, 0, 0xd0000, 0x4000, -1, -1,
	BUS_ISA, 0x3e0, 0, 0xd0000, 0x4000, -1, -1, BUS_ISA,
	-1, 1, 0,
};

/* parent vectors */
static short pv[48] = {
	17, 19, 18, 25, 39, 40, 41, 42, 45, 43, 46, 44, 47, 48, 49, 50,
	51, 52, 53, 118, 119, 120, -1, 21, 22, 23, 24, -1, 68, 71, 83, 88,
	-1, 22, 23, -1, 0, -1, 68, -1, 14, -1, 71, -1, 20, -1, 21, -1,
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

struct cfdata cfdata[] = {
	/* driver     unit state    loc     flags parents ivstubs */
/*  0: isa0 at root */
	{&isacd,	 0, NORM,     loc,      0, pv+22, 0},
/*  1: saturn0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&saturncd,	 0, NORM, loc+  7,      0, pv+36, 0},
/*  2: pccons0 at isa0 port IO_KBD nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&pcconscd,	 0, NORM, loc+ 14,      0, pv+36, 0},
/*  3: pcaux0 at isa0 port IO_KBD nports 0 iomem 0 iosiz 0 irq 0xc drq -1 bustype BUS_ISA */
	{&pcauxcd,	 0, NORM, loc+ 21,      0, pv+36, 0},
/*  4: pcic0 at isa0 port 0x3e0 nports 0 iomem 0 iosiz 0 irq 0xb drq -1 bustype BUS_ISA */
	{&pciccd,	 0, NORM, loc+ 28,      0, pv+36, 0},
/*  5: pcic1 at isa0 port 0x3e2 nports 0 iomem 0 iosiz 0 irq 0xa drq -1 bustype BUS_ISA */
	{&pciccd,	 1, NORM, loc+ 35,      0, pv+36, 0},
/*  6: pcic1 at isa0 port 0x3e4 nports 0 iomem 0 iosiz 0 irq 0xa drq -1 bustype BUS_ISA */
	{&pciccd,	 1, NORM, loc+ 42,      0, pv+36, 0},
/*  7: com0 at isa0 port IO_COM1 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&comcd,	 0, NORM, loc+ 49,      0, pv+36, 0},
/*  8: com1 at isa0 port IO_COM2 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&comcd,	 1, NORM, loc+ 56,      0, pv+36, 0},
/*  9: com2 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&comcd,	 2, NORM, loc+ 63,      0, pv+36, 0},
/* 10: com3 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&comcd,	 3, NORM, loc+ 63,      0, pv+36, 0},
/* 11: lp0 at isa0 port 0x378 nports 0 iomem 0 iosiz 0 irq 7 drq -1 bustype BUS_ISA */
	{&lpcd,		 0, NORM, loc+ 70,    0x1, pv+36, 0},
/* 12: lp0 at isa0 port 0x3bc nports 0 iomem 0 iosiz 0 irq 7 drq -1 bustype BUS_ISA */
	{&lpcd,		 0, NORM, loc+ 77,    0x1, pv+36, 0},
/* 13: lp2 at isa0 port 0x3bc nports 0 iomem 0 iosiz 0 irq 7 drq -1 bustype BUS_ISA */
	{&lpcd,		 2, NORM, loc+ 77,    0x1, pv+36, 0},
/* 14: fdc0 at isa0 port IO_FD1 nports 0 iomem 0 iosiz 0 irq -1 drq 2 bustype BUS_ISA */
	{&fdccd,	 0, NORM, loc+ 84,      0, pv+36, 0},
/* 15: fd0 at fdc0 drive 0 */
	{&fdcd,		 0, NORM, loc+394,      0, pv+40, 0},
/* 16: fd1 at fdc0 drive 1 */
	{&fdcd,		 1, NORM, loc+393,      0, pv+40, 0},
/* 17: dpt0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ANY */
	{&dptcd,	 0, NORM, loc+161,      0, pv+36, 0},
/* 18: dpt* at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ANY */
	{&dptcd,	 1, STAR, loc+161,      0, pv+36, 0},
/* 19: dpt0 at isa0 port 0x170 nports 0 iomem 0 iosiz 0 irq -1 drq 5 bustype BUS_ISA */
	{&dptcd,	 0, NORM, loc+168,      0, pv+36, 0},
/* 20: tg* at dpt0|dpt0|dpt*|wdpi*|ncr0|ncr1|ncr2|aic0|aic0|aic1|aic1|aic2|aic2|bha0|aha0|bha1|aha1|sa0|sa0|eaha0|eaha1|eaha2 target -1 */
	{&tgcd,		 0, STAR, loc+392,      0, pv+ 0, 0},
/* 21: wdc0 at isa0 port IO_WD1 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wdccd,	 0, NORM, loc+175,      0, pv+36, 0},
/* 22: wdc1 at isa0 port IO_WD2 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wdccd,	 1, NORM, loc+182,      0, pv+36, 0},
/* 23: wdc1 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&wdccd,	 1, NORM, loc+ 63,      0, pv+36, 0},
/* 24: wdc2 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&wdccd,	 2, NORM, loc+ 63,      0, pv+36, 0},
/* 25: wdpi* at wdc0|wdc1|wdc1|wdc2 drive -1 */
	{&wdpicd,	 0, STAR, loc+392,      0, pv+23, 0},
/* 26: wd0 at wdc0 drive 0 */
	{&wdcd,		 0, NORM, loc+394,      0, pv+46, 0},
/* 27: wd1 at wdc0 drive 1 */
	{&wdcd,		 1, NORM, loc+393,      0, pv+46, 0},
/* 28: wd2 at wdc1|wdc1 drive 0 */
	{&wdcd,		 2, NORM, loc+394,      0, pv+33, 0},
/* 29: wd3 at wdc1|wdc1 drive 1 */
	{&wdcd,		 3, NORM, loc+393,      0, pv+33, 0},
/* 30: wd4 at wdc2 drive 0 */
	{&wdcd,		 4, NORM, loc+394,      0, pv+26, 0},
/* 31: wd5 at wdc2 drive 1 */
	{&wdcd,		 5, NORM, loc+393,      0, pv+26, 0},
/* 32: wt0 at isa0 port 0x300 nports 0 iomem 0 iosiz 0 irq -1 drq 1 bustype BUS_ISA */
	{&wtcd,		 0, NORM, loc+189,      0, pv+36, 0},
/* 33: npx0 at isa0 port IO_NPX nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&npxcd,	 0, NORM, loc+196,      0, pv+36, 0},
/* 34: vga0 at isa0 port IO_VGA nports 0 iomem 0xa0000 iosiz 0x10000 irq -1 drq -1 bustype BUS_ISA */
	{&vgacd,	 0, NORM, loc+203,      0, pv+36, 0},
/* 35: bms0 at isa0 port 0x23c nports 0 iomem 0 iosiz 0 irq 5 drq -1 bustype BUS_ISA */
	{&bmscd,	 0, NORM, loc+210,      0, pv+36, 0},
/* 36: lms0 at isa0 port 0x23c nports 0 iomem 0 iosiz 0 irq 5 drq -1 bustype BUS_ISA */
	{&lmscd,	 0, NORM, loc+210,      0, pv+36, 0},
/* 37: mcd0 at isa0 port 0x334 nports 0 iomem 0 iosiz 0 irq 9 drq -1 bustype BUS_ISA */
	{&mcdcd,	 0, NORM, loc+217,      0, pv+36, 0},
/* 38: mcd0 at isa0 port 0x340 nports 0 iomem 0 iosiz 0 irq 9 drq -1 bustype BUS_ISA */
	{&mcdcd,	 0, NORM, loc+224,      0, pv+36, 0},
/* 39: ncr0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&ncrcd,	 0, NORM, loc+  7,      0, pv+36, 0},
/* 40: ncr1 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&ncrcd,	 1, NORM, loc+  7,      0, pv+36, 0},
/* 41: ncr2 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&ncrcd,	 2, NORM, loc+  7,      0, pv+36, 0},
/* 42: aic0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&aiccd,	 0, NORM, loc+  7,      0, pv+36, 0},
/* 43: aic1 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&aiccd,	 1, NORM, loc+  7,      0, pv+36, 0},
/* 44: aic2 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&aiccd,	 2, NORM, loc+  7,      0, pv+36, 0},
/* 45: aic0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&aiccd,	 0, NORM, loc+  0,      0, pv+36, 0},
/* 46: aic1 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&aiccd,	 1, NORM, loc+  0,      0, pv+36, 0},
/* 47: aic2 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&aiccd,	 2, NORM, loc+  0,      0, pv+36, 0},
/* 48: bha0 at isa0 port 0x330 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&bhacd,	 0, NORM, loc+126,      0, pv+36, 0},
/* 49: aha0 at isa0 port 0x330 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&ahacd,	 0, NORM, loc+126,      0, pv+36, 0},
/* 50: bha1 at isa0 port 0x334 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&bhacd,	 1, NORM, loc+231,      0, pv+36, 0},
/* 51: aha1 at isa0 port 0x334 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&ahacd,	 1, NORM, loc+231,      0, pv+36, 0},
/* 52: sa0 at isa0 port 0x340 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&sacd,		 0, NORM, loc+238,      0, pv+36, 0},
/* 53: sa0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&sacd,		 0, NORM, loc+ 63,      0, pv+36, 0},
/* 54: sr* at tg* unit -1 */
	{&srcd,		 0, STAR, loc+392,      0, pv+44, 0},
/* 55: sd* at tg* unit -1 */
	{&sdcd,		 0, STAR, loc+392,      0, pv+44, 0},
/* 56: st* at tg* unit -1 */
	{&stcd,		 0, STAR, loc+392,      0, pv+44, 0},
/* 57: sg* at tg* unit -1 */
	{&sgcd,		 0, STAR, loc+392,      0, pv+44, 0},
/* 58: ne0 at isa0 port 0x320 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&necd,		 0, NORM, loc+245,      0, pv+36, 0},
/* 59: ne0 at isa0 port 0x340 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&necd,		 0, NORM, loc+238,      0, pv+36, 0},
/* 60: ne0 at isa0 port 0x360 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&necd,		 0, NORM, loc+252,      0, pv+36, 0},
/* 61: ne0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&necd,		 0, NORM, loc+ 63,      0, pv+36, 0},
/* 62: ep0 at isa0 port 0x240 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&epcd,		 0, NORM, loc+259,      0, pv+36, 0},
/* 63: ep0 at isa0 port 0x320 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&epcd,		 0, NORM, loc+245,      0, pv+36, 0},
/* 64: tn0 at isa0 port 0x320 nports 0 iomem 0 iosiz 0 irq -1 drq 3 bustype BUS_ISA */
	{&tncd,		 0, NORM, loc+266,      0, pv+36, 0},
/* 65: tn0 at isa0 port 0x340 nports 0 iomem 0 iosiz 0 irq -1 drq 3 bustype BUS_ISA */
	{&tncd,		 0, NORM, loc+273,      0, pv+36, 0},
/* 66: tn0 at isa0 port 0x360 nports 0 iomem 0 iosiz 0 irq -1 drq 3 bustype BUS_ISA */
	{&tncd,		 0, NORM, loc+280,      0, pv+36, 0},
/* 67: tn0 at isa0 port 0x300 nports 0 iomem 0 iosiz 0 irq -1 drq 3 bustype BUS_ISA */
	{&tncd,		 0, NORM, loc+287,      0, pv+36, 0},
/* 68: de* at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ANY */
	{&decd,		 0, STAR, loc+161,      0, pv+36, 0},
/* 69: nsphy* at de*|exp*|eb*|tl* phy -1 */
	{&nsphycd,	 0, STAR, loc+392,      0, pv+28, 0},
/* 70: ics90p* at de* phy -1 */
	{&ics90pcd,	 0, STAR, loc+392,      0, pv+38, 0},
/* 71: exp* at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&expcd,	 0, STAR, loc+  7,      0, pv+36, 0},
/* 72: i555p* at exp* phy -1 */
	{&i555pcd,	 0, STAR, loc+392,      0, pv+42, 0},
/* 73: hpp0 at isa0 port 0x2c0 nports 0 iomem 0xc8000 iosiz 0x800 irq -1 drq -1 bustype BUS_ISA */
	{&hppcd,	 0, NORM, loc+294,      0, pv+36, 0},
/* 74: hpp0 at isa0 port 0x300 nports 0 iomem 0xc8000 iosiz 0x800 irq -1 drq -1 bustype BUS_ISA */
	{&hppcd,	 0, NORM, loc+301,      0, pv+36, 0},
/* 75: re0 at isa0 port 0x240 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+259,      0, pv+36, 0},
/* 76: re0 at isa0 port 0x260 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+147,      0, pv+36, 0},
/* 77: re0 at isa0 port 0x280 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+ 91,      0, pv+36, 0},
/* 78: re0 at isa0 port 0x2a0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+ 98,      0, pv+36, 0},
/* 79: re0 at isa0 port 0x300 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+112,      0, pv+36, 0},
/* 80: re0 at isa0 port 0x320 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+245,      0, pv+36, 0},
/* 81: re0 at isa0 port 0x340 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+238,      0, pv+36, 0},
/* 82: re0 at isa0 port 0x380 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&recd,		 0, NORM, loc+308,      0, pv+36, 0},
/* 83: eb* at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&ebcd,		 0, STAR, loc+  7,      0, pv+36, 0},
/* 84: ef0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&efcd,		 0, NORM, loc+  7,      0, pv+36, 0},
/* 85: ef1 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&efcd,		 1, NORM, loc+  7,      0, pv+36, 0},
/* 86: ef* at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&efcd,		 3, STAR, loc+  7,      0, pv+36, 0},
/* 87: ne* at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&necd,		 1, STAR, loc+  7,      0, pv+36, 0},
/* 88: tl* at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCI */
	{&tlcd,		 0, STAR, loc+  7,      0, pv+36, 0},
/* 89: tn100a* at tl* phy -1 */
	{&tn100acd,	 0, STAR, loc+392,      0, pv+31, 0},
/* 90: we0 at isa0 port 0x280 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+315,      0, pv+36, 0},
/* 91: we0 at isa0 port 0x2a0 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+322,      0, pv+36, 0},
/* 92: we0 at isa0 port 0x2c0 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+329,      0, pv+36, 0},
/* 93: we0 at isa0 port 0x2e0 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+336,      0, pv+36, 0},
/* 94: we0 at isa0 port 0x300 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+343,      0, pv+36, 0},
/* 95: we0 at isa0 port 0x320 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+350,      0, pv+36, 0},
/* 96: we0 at isa0 port 0x340 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+357,      0, pv+36, 0},
/* 97: we0 at isa0 port 0x360 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+364,      0, pv+36, 0},
/* 98: we0 at isa0 port 0x380 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+371,      0, pv+36, 0},
/* 99: we0 at isa0 port 0x3a0 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+378,      0, pv+36, 0},
/*100: we0 at isa0 port 0x3e0 nports 0 iomem 0xd0000 iosiz 0x4000 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+385,      0, pv+36, 0},
/*101: we0 at isa0 port 0x280 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+ 91,      0, pv+36, 0},
/*102: we0 at isa0 port 0x2a0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+ 98,      0, pv+36, 0},
/*103: we0 at isa0 port 0x2e0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+105,      0, pv+36, 0},
/*104: we0 at isa0 port 0x300 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+112,      0, pv+36, 0},
/*105: we0 at isa0 port 0x310 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+119,      0, pv+36, 0},
/*106: we0 at isa0 port 0x330 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+126,      0, pv+36, 0},
/*107: we0 at isa0 port 0x350 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&wecd,		 0, NORM, loc+133,      0, pv+36, 0},
/*108: ef0 at isa0 port 0x250 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&efcd,		 0, NORM, loc+140,      0, pv+36, 0},
/*109: ef1 at isa0 port 0x260 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&efcd,		 1, NORM, loc+147,      0, pv+36, 0},
/*110: ef0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&efcd,		 0, NORM, loc+  0,      0, pv+36, 0},
/*111: ef1 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&efcd,		 1, NORM, loc+  0,      0, pv+36, 0},
/*112: ef2 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&efcd,		 2, NORM, loc+  0,      0, pv+36, 0},
/*113: ef0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&efcd,		 0, NORM, loc+ 63,      0, pv+36, 0},
/*114: el0 at isa0 port 0x310 nports 0 iomem 0xd0000 iosiz 0x10000 irq -1 drq -1 bustype BUS_ISA */
	{&elcd,		 0, NORM, loc+154,      0, pv+36, 0},
/*115: ex0 at isa0 port 0x260 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_ISA */
	{&excd,		 0, NORM, loc+147,      0, pv+36, 0},
/*116: cce0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&ccecd,	 0, NORM, loc+ 63,      0, pv+36, 0},
/*117: mz0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_PCMCIA */
	{&mzcd,		 0, NORM, loc+ 63,      0, pv+36, 0},
/*118: eaha0 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&eahacd,	 0, NORM, loc+  0,      0, pv+36, 0},
/*119: eaha1 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&eahacd,	 1, NORM, loc+  0,      0, pv+36, 0},
/*120: eaha2 at isa0 port 0 nports 0 iomem 0 iosiz 0 irq -1 drq -1 bustype BUS_EISA */
	{&eahacd,	 2, NORM, loc+  0,      0, pv+36, 0},
	{0}
};

short cfroots[] = {
	 0 /* isa0 */,
	-1
};

/* pseudo-devices */
extern void loopattach __P((int));
extern void ptyattach __P((int));
extern void slattach __P((int));
extern void apppattach __P((int));
extern void pifattach __P((int));
extern void bpfilterattach __P((int));
extern void rdattach __P((int));
extern void tunattach __P((int));
extern void apmattach __P((int));
extern void ssattach __P((int));
extern void csattach __P((int));
extern void vndattach __P((int));

struct pdevinit pdevinit[] = {
	{ loopattach, 1 },
	{ ptyattach, 1 },
	{ slattach, 1 },
	{ apppattach, 2 },
	{ pifattach, 1 },
	{ bpfilterattach, 10 },
	{ rdattach, 1 },
	{ tunattach, 2 },
	{ apmattach, 1 },
	{ ssattach, 4 },
	{ csattach, 1 },
	{ vndattach, 2 },
	{ 0, 0 }
};
#line 28 "../../i386/conf/ioconf.c.i386"


extern struct devsw cnsw, cttysw, mmsw, swapsw, logsw, devfdsw;
extern struct devsw ptssw, ptcsw;
extern struct devsw pcsw, kbdsw;
#if defined(IPFILTER)
extern struct devsw iplsw;
#endif

extern struct devsw tgsw;
extern struct devsw sdsw;
extern struct devsw srsw;
extern struct devsw stsw;
extern struct devsw sgsw;
extern struct devsw apppsw;
extern struct devsw pifsw;
extern struct devsw bpfiltersw;
extern struct devsw loopsw;
extern struct devsw tunsw;
extern struct devsw ptysw;
extern struct devsw slsw;
extern struct devsw vndsw;
extern struct devsw rdsw;
extern struct devsw isasw;
extern struct devsw pcconssw;
extern struct devsw pcauxsw;
extern struct devsw comsw;
extern struct devsw lpsw;
extern struct devsw fdcsw;
extern struct devsw fdsw;
extern struct devsw dptsw;
extern struct devsw wdcsw;
extern struct devsw wdsw;
extern struct devsw wdpisw;
extern struct devsw mcdsw;
extern struct devsw wtsw;
extern struct devsw npxsw;
extern struct devsw vgasw;
extern struct devsw bmssw;
extern struct devsw lmssw;
extern struct devsw ahasw;
extern struct devsw bhasw;
extern struct devsw nesw;
extern struct devsw epsw;
extern struct devsw sasw;
extern struct devsw ncrsw;
extern struct devsw saturnsw;
extern struct devsw aicsw;
extern struct devsw tnsw;
extern struct devsw hppsw;
extern struct devsw resw;
extern struct devsw wesw;
extern struct devsw tlsw;
extern struct devsw ebsw;
extern struct devsw efsw;
extern struct devsw elsw;
extern struct devsw exsw;
extern struct devsw eahasw;
extern struct devsw apmsw;
extern struct devsw sssw;
extern struct devsw cssw;
extern struct devsw pcicsw;
extern struct devsw ccesw;
extern struct devsw mzsw;
extern struct devsw desw;
extern struct devsw expsw;
extern struct devsw nsphysw;
extern struct devsw ics90psw;
extern struct devsw i555psw;
extern struct devsw tn100asw;
#line 37 "../../i386/conf/ioconf.c.i386"


struct devsw *devsw[] = {
	&cnsw,			/* 0 = virtual console */
	&cttysw,		/* 1 = controlling terminal */
	&mmsw,			/* 2 = /dev/{null,mem,kmem,...} */
	&wdsw,		/* 3 = st506/rll/esdi/ide disk */
	&swapsw,		/* 4 = /dev/drum (swap pseudo-device) */
	&ptssw,	/* 5 = pseudo-tty slave */
	&ptcsw,	/* 6 = pseudo-tty master */
	&logsw,			/* 7 = /dev/klog */
	&comsw,		/* 8 = serial communications ports */
	&fdsw,		/* 9 = floppy disk */
	&wtsw,		/* 10 = QIC-02/36 cartridge tape */
	NULL,		/* 11 = RISCom/N8 async mux */
	&pcsw,	/* 12 = vga console */
	&pcauxsw,		/* 13 = console/keyboard aux port */
	&bpfiltersw,	/* 14 = berkeley packet filter */
	&devfdsw,		/* 15 = file descriptor devices */
	&vgasw,		/* 16 = VGA display for X */
	&kbdsw,	/* 17 = Keyboard device (excl from cn) */
	&sdsw,		/* 18 = SCSI disk pseudo-device (sd) */
	&stsw,		/* 19 = SCSI tape pseudo-device */
	&lpsw,		/* 20 = printer on a parallel port */
	&bmssw,		/* 21 = Microsoft Bus Mouse */
	NULL,			/* 22 = Midi device (RETIRED) */
	&mcdsw,		/* 23 = Mitsumi CD-ROM */
	NULL,		/* 24 = Maxpeed Async Mux */
	&lmssw,		/* 25 = Logitec Bus Mouse */
	NULL,		/* 26 = DigiBoard PC/X[ei] */
	NULL,		/* 27 = Specialix multiplexor */
	NULL,			/* 28 = SoundBlaster Pro (RETIRED) */
	NULL,		/* 29 = Chase IOPRO control driver */
	NULL,		/* 30 = Chase IOPRO data driver */
	NULL,		/* 31 = Equinox tty */
	NULL,		/* 32 = Concatenated disk pseudo-device */
	NULL,		/* 33 = Voxware sound system */
	&srsw,		/* 34 = SCSI removeable disks (clone of sd) */
	NULL,		/* 35 = Comtrol Rocketport */
	NULL,		/* 36 = Cyclades async mux */
	NULL,		/* 37 = Disk splicing driver */
	&dptsw,            /* 38 = DPT config driver */
	&sgsw,		/* 39 = SCSI generic driver, catch all */
	NULL,		/* 40 = Stallion async mux */
	NULL,		/* 41 = Stallion intelligent async mux */
	&vndsw,		/* 42 = vnode disk driver */
	NULL,		/* 43 = Connectix QuickCam */
	&rdsw,		/* 44 = ram disk driver */
	&tunsw,		/* 45 = Tunnel Network Interface */
#if defined(IPFILTER)
	&iplsw,			/* 46 = IP Filter */
#else
	NULL,			/* 46 = (unused) */
#endif
	NULL,			/* 47 = (unused) */
	NULL,			/* 48 = (unused) */
	NULL,			/* 49 = (unused) */
	&apmsw,		/* 50 = APM Interface module */
	&cssw,		/* 51 = PCMCIA CS Interface module */
	NULL,		/* 52 = Focus Video Capture */
	NULL,		/* 53 = PCMCIA SRAM Drive */
};
#define	NDEVSW (sizeof(devsw) / sizeof(*devsw))
int	ndevsw = NDEVSW;

#ifdef COMPAT_DEV
/* cross-correlation to devsw[] above, from old bdevsw index */
/* (i.e., devsw[blktodev[i]] is the driver for old block device i) */
int	blktodev[7] = {
	3,			/* 0 = wd = 3 */
	4,			/* 1 = swap = 4 */
	9,			/* 2 = floppy = 9 */
	10,			/* 3 = wt = 10 */
	18,			/* 4 = sd = 18 */
	19,			/* 5 = st = 19 */
	23,			/* 6 = mcd = 23 */
};
#endif

/*
 * Swapdev is a fake device implemented
 * in vm_swap.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(4, 0);

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == 2 && (minor(dev) == 0 || minor(dev) == 1));
}

iszerodev(dev)
	dev_t dev;
{
	return (major(dev) == 2 && minor(dev) == 12);
}

#ifdef COMPAT_DEV
#include <sys/vnode.h>

int
devcompat(dev, type)
	dev_t dev;
	int type;
{
	int maj, min, unit = 0;

	if (dev > USHRT_MAX)
		return (dev);
	maj = dev >> 8;
	min = dev & 0xff;
	if (type == VBLK && maj < sizeof(blktodev) / sizeof(blktodev[0]))
		maj = blktodev[maj];
	switch (maj) {
	case 3:				/* 0 = wd = 3 */
	case 9:				/* 2 = floppy = 9 */
	case 18:			/* 4 = sd = 18 */
	case 23:			/* 6 = mcd = 23 */
		unit = min >> 3;		/* drive */
		min &= 0x7;			/* partition */
		break;

	case 10:			/* 3 = wt = 10 */
	case 19:			/* 5 = st = 19 */
		unit = min & 3;			/* unit */
		min >>= 2;			/* rewind, density */
		break;
	}
	return (dv_makedev(maj, unit, min));
}
#endif
