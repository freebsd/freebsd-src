/*
 * Intel 8237 DMA Controller
 *
 * $FreeBSD: src/sys/dev/ic/i8237.h,v 1.10 2005/05/14 10:14:56 nyan Exp $
 */

#define	DMA37MD_SINGLE	0x40	/* single pass mode */
#define	DMA37MD_CASCADE	0xc0	/* cascade mode */
#define	DMA37MD_AUTO	0x50	/* autoinitialise single pass mode */
#define	DMA37MD_WRITE	0x04	/* read the device, write memory operation */
#define	DMA37MD_READ	0x08	/* write the device, read memory operation */
