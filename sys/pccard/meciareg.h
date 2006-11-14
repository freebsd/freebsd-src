/*
 * meciareg.h
 *
 * PC9801 original PCMCIA controller code for NS/A,Ne,NX/C,NR/L.
 * by Noriyuki Hosobuchi <hoso@ce.mbn.or.jp>
 *
 * $FreeBSD$
 */

/*--- I/O port definition */
#define	MECIA_REG0		0x0a8e	/* byte */
#define	MECIA_REG1		0x1a8e	/* byte */
#define	MECIA_REG2		0x2a8e	/* byte */
#define	MECIA_REG3		0x3a8e	/* byte : Interrupt */
#define	MECIA_REG4		0x4a8e	/* word : PC98 side I/O base */
#define	MECIA_REG5		0x5a8e	/* word : Card side I/O base */
#define	MECIA_REG7		0x7a8e	/* byte */

#define	MECIA_REG_WINSEL	0x1e8e	/* byte : win bank select register */
#define	MECIA_REG_PAGOFS	0x0e8e	/* word */

/* PC98_REG_WINSEL */
#define	MECIA_MAPWIN		0x84	/* map Card on 0xda0000 - 0xdbffff */
#define	MECIA_UNMAPWIN		0x00

/* MECIA_REG1 */
#define	MECIA_CARDEXIST		0x08	/* 1:exist 0:not exist */

/* MECIA_REG2 */
#define	MECIA_MAPIO		0x80	/* 1:I/O 0:Memory */
#define	MECIA_IOTHROUGH		0x40	/* 0:I/O map 1:I/O addr-through */
#define	MECIA_8BIT		0x20	/* bit width 1:8bit 0:16bit */
#define	MECIA_MAP128		0x10	/* I/O map size 1:128byte 0:16byte */
#define	MECIA_VCC3P3V		0x02	/* Vcc 1:3.3V 0:5.0V */

/* MECIA_REG3 */
#define	MECIA_INT0		(0xf8 + 0x0)	/* INT0(IRQ3) */
#define	MECIA_INT1		(0xf8 + 0x1)	/* INT1(IRQ5) */
#define	MECIA_INT2		(0xf8 + 0x2)	/* INT2(IRQ6) */
#define	MECIA_INT4		(0xf8 + 0x4)	/* INT4(IRQ10) */
#define	MECIA_INT5		(0xf8 + 0x5)	/* INT5(IRQ12) */
#define	MECIA_INTDISABLE	(0xf8 + 0x7)	/* disable interrupt */

/* MECIA_REG7 */
#define	MECIA_ATTRMEM		0x20	/* 1:attr mem 0:common mem */
#define	MECIA_VPP12V		0x10	/* Vpp 0:5V 1:12V */


#ifdef KERNEL
extern int mecia_mode;		/* in 'pccard/pcic.c' */
#define mecia_8bit_on()	\
	if (mecia_mode & MECIA_8BIT)	\
		outb(MECIA_REG2, inb(MECIA_REG2) | MECIA_8BIT)
#define mecia_8bit_off()	\
	if (mecia_mode & MECIA_8BIT)	\
		outb(MECIA_REG2, inb(MECIA_REG2) & ~MECIA_8BIT)
#define mecia_map128()		(mecia_mode & MECIA_MAP128)
#endif

#define	MECIA_INT_MASK_ALLOWED	0x3E68		/* PC98 */
