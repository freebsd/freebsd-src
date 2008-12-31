/*
 * $FreeBSD: src/sys/dev/ie/if_ie507.h,v 1.5.30.1 2008/11/25 02:59:29 kensmith Exp $
 * Definitions for 3C507
 */

#define	IE507_CTRL	6	/* control port */
#define	IE507_ICTRL	10	/* interrupt control */
#define	IE507_ATTN 	11	/* any write here sends a chan attn */
#define	IE507_ROM	13	/* rom configuration? */
#define	IE507_MADDR	14	/* shared memory configuration */
#define	IE507_IRQ	15	/* IRQ configuration */

/* Bank 0 -- "*3COM*" signature */
#define	EL_SIG		0	/* offset of ASCII signature -- "*3COM*" */

/* Bank 1 -- ethernet address */
#define	EL_ADDR		0	/* offset of card's ethernet address */

/* Bank 2 -- card part #, revision, date of manufacture */
#define	EL_TYPE		0	/* offset of card part # */
#define	EL_TYPE_HI	0	/* offset of card part # -- high byte */
#define	EL_TYPE_MID	1	/* offset of card part # -- middle byte */
#define	EL_TYPE_LOW	2	/* offset of card part # -- low byte */
#define	EL_REV		3	/* offset of card revision, in BCD */
#define	EL_DOM_DAY	4	/* offset of date of manf: day in BCD */
#define	EL_DOM_MY	4	/* offset of date of manf: month, year in BCD */

/*
 * Definitions for non-bankswitched registers
 */

/* General control register */
#define	EL_CTRL_BNK0	0x00	/* register bank 0 */
#define	EL_CTRL_BNK1	0x01	/* register bank 1 */
#define	EL_CTRL_BNK2	0x02	/* register bank 2 */
#define	EL_CTRL_IEN	0x04	/* interrupt enable */
#define	EL_CTRL_INTL	0x08	/* interrupt active latch */
#define	EL_CTRL_16BIT	0x10	/* bus width; clear = 8-bit, set = 16-bit */
#define	EL_CTRL_LOOP	0x20	/* loopback mode */
#define	EL_CTRL_NRST	0x80	/* turn off to reset */
#define	EL_CTRL_RESET	(EL_CTRL_LOOP)
#define	EL_CTRL_NORMAL	(EL_CTRL_NRST | EL_CTRL_IEN | EL_CTRL_BNK1)

/* ROM & media control register */
#define	EL_MEDIA_MASK	0x80	/* m1 = (EL_MEDIA register) & EL_MEDIA_MASK */
#define	EL_MEDIA_SHIFT	7	/* media index = m1 >> EL_MEDIA_SHIFT */

/* shared memory control register */
#define	EL_MADDR_HIGH	0x20	/* memory mapping above 15Meg */
#define	EL_MADDR_MASK	0x1c	/* m1 = (EL_MADDR register) & EL_MADDR_MASK */
#define	EL_MADDR_SHIFT	12	/* m2 = m1 << EL_MADDR_SHIFT  */
#define	EL_MADDR_BASE	0xc0000	/* maddr = m2 + EL_MADDR_BASE */
#define	EL_MSIZE_MASK	0x03	/* m1 = (EL_MADDR register) & EL_MSIZE_MASK */
#define	EL_MSIZE_STEP	16384	/* msize = (m1 + 1) * EL_MSIZE_STEP */

/* interrupt control register */
#define	EL_IRQ_MASK	0x0f	/* irq = (EL_IRQ register) & EL_IRQ_MASK */

/*
 * Definitions for Bank 0 registers
 */
#define	EL_SIG_LEN	6	/* signature length */
#define	EL_SIGNATURE	"*3COM*"

/*
 * Definitions for Bank 1 registers
 */
#define	EL_ADDR_LEN	6	/* ether address length */

/*
 * Definitions for Bank 2 registers
 */
#define	EL_TYPE_LEN	3	/* card part # length */

/*
 * General card-specific macros and definitions
 */
#define	EL_IOBASE_LOW	0x200
#define	EL_IOBASE_HIGH	0x3e0
#define	EL_IOSIZE	16

/*
 * XXX: It seems that the 3C507-TP is differentiated from AUI/BNC 3C507
 * by part numbers, but I'm not sure how accurate this test is, seeing
 * as it's based on the sample of 3 cards I own (2AUI/BNC, 1 TP).
 */
#define	EL_IS_TP(type)	((type)[EL_TYPE_MID] > 0x70)

#define	EL_CARD_BNC	0	/* regular AUI/BNC 3C507 */
#define	EL_CARD_TP	1	/* 3C507-TP -- no AUI/BNC */
