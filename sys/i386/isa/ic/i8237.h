/*
 * Intel 8237 DMA Controller
 *
 *	$Id: i8237.h,v 1.2 1993/10/16 13:48:48 rgrimes Exp $
 */

#define	DMA37MD_SINGLE	0x40	/* single pass mode */
#define	DMA37MD_CASCADE	0xc0	/* cascade mode */
#define	DMA37MD_WRITE	0x04	/* read the device, write memory operation */
#define	DMA37MD_READ	0x08	/* write the device, read memory operation */
	
