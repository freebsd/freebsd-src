/*
 * pcic98reg.h
 *
 * PC9801 original PCMCIA controller code for NS/A,Ne,NX/C,NR/L.
 * by Noriyuki Hosobuchi <hoso@ce.mbn.or.jp>
 *
 * $FreeBSD$
 */

/*--- I/O port definition */
#define	PCIC98_REG0		0x0a8e	/* byte */
#define	PCIC98_REG1		0x1a8e	/* byte */
#define	PCIC98_REG2		0x2a8e	/* byte */
#define	PCIC98_REG3		0x3a8e	/* byte : Interrupt */
#define	PCIC98_REG4		0x4a8e	/* word : PC98 side I/O base */
#define	PCIC98_REG5		0x5a8e	/* word : Card side I/O base */
#define	PCIC98_REG7		0x7a8e	/* byte */

#define	PCIC98_REG_WINSEL	0x1e8e	/* byte : win bank select register */
#define	PCIC98_REG_PAGOFS	0x0e8e	/* word */

/* PC98_REG_WINSEL */
#define	PCIC98_MAPWIN		0x84	/* map Card on 0xda0000 - 0xdbffff */
#define	PCIC98_UNMAPWIN		0x00

/* PCIC98_REG1 */
#define	PCIC98_CARDEXIST	0x08	/* 1:exist 0:not exist */

/* PCIC98_REG2 */
#define	PCIC98_MAPIO		0x80	/* 1:I/O 0:Memory */
#define	PCIC98_IOTHROUGH	0x40	/* 0:I/O map 1:I/O addr-through */
#define	PCIC98_8BIT		0x20	/* bit width 1:8bit 0:16bit */
#define	PCIC98_MAP128		0x10	/* I/O map size 1:128byte 0:16byte */
#define	PCIC98_VCC3P3V		0x02	/* Vcc 1:3.3V 0:5.0V */

/* PCIC98_REG3 */
#define	PCIC98_INT0		(0xf8 + 0x0)	/* INT0(IRQ3) */
#define	PCIC98_INT1		(0xf8 + 0x1)	/* INT1(IRQ5) */
#define	PCIC98_INT2		(0xf8 + 0x2)	/* INT2(IRQ6) */
#define	PCIC98_INT4		(0xf8 + 0x4)	/* INT4(IRQ10) */
#define	PCIC98_INT5		(0xf8 + 0x5)	/* INT5(IRQ12) */
#define	PCIC98_INTDISABLE	(0xf8 + 0x7)	/* disable interrupt */

/* PCIC98_REG7 */
#define	PCIC98_ATTRMEM		0x20	/* 1:attr mem 0:common mem */
#define	PCIC98_VPP12V		0x10	/* Vpp 0:5V 1:12V */


#ifdef KERNEL
extern int pcic98_mode;		/* in 'pccard/pcic.c' */
#define pcic98_8bit_on()	\
	if (pcic98_mode & PCIC98_8BIT)	\
		outb(PCIC98_REG2, inb(PCIC98_REG2) | PCIC98_8BIT)
#define pcic98_8bit_off()	\
	if (pcic98_mode & PCIC98_8BIT)	\
		outb(PCIC98_REG2, inb(PCIC98_REG2) & ~PCIC98_8BIT)
#define pcic98_map128()		(pcic98_mode & PCIC98_MAP128)
#endif
