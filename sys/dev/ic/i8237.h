/*
 * Intel 8237 DMA Controller
 *
 * $FreeBSD$
 */

#define	DMA37MD_SINGLE	0x40	/* single pass mode */
#define	DMA37MD_CASCADE	0xc0	/* cascade mode */
#define	DMA37MD_AUTO	0x50	/* autoinitialise single pass mode */
#define	DMA37MD_WRITE	0x04	/* read the device, write memory operation */
#define	DMA37MD_READ	0x08	/* write the device, read memory operation */

#ifndef PC98
#define	DMA1_STATUS	(IO_DMA1 + 1*8)	/* status register */
#define	DMA2_STATUS	(IO_DMA2 + 2*8)	/* status register */
#endif
