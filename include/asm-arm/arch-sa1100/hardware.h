/*
 * linux/include/asm-arm/arch-sa1100/hardware.h
 *
 * Copyright (C) 1998 Nicolas Pitre <nico@cam.org>
 *
 * This file contains the hardware definitions for SA1100 architecture
 *
 * 2000/05/23 John Dorsey <john+@cs.cmu.edu>
 *      Definitions for SA1111 added.
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>
#include <asm/mach-types.h>


/* Flushing areas */
#define FLUSH_BASE_PHYS		0xe0000000	/* SA1100 zero bank */
#define FLUSH_BASE		0xf5000000
#define FLUSH_BASE_MINICACHE	0xf5800000
#define UNCACHEABLE_ADDR	0xfa050000


/*
 * Those are statically mapped PCMCIA IO space for designs using it as a
 * generic IO bus, typically with ISA parts, hardwired IDE interfaces, etc.
 * The actual PCMCIA code is mapping required IO region at run time.
 */
#define PCMCIA_IO_0_BASE	0xf6000000
#define PCMCIA_IO_1_BASE	0xf7000000


/*
 * We requires absolute addresses i.e. (PCMCIA_IO_0_BASE + 0x3f8) for 
 * in*()/out*() macros to be usable for all cases.
 */
#define PCIO_BASE		0


/*
 * SA1100 internal I/O mappings
 *
 * We have the following mapping:
 *      phys            virt
 *      80000000        f8000000
 *      90000000        fa000000
 *      a0000000        fc000000
 *      b0000000        fe000000
 */

#define VIO_BASE        0xf8000000	/* virtual start of IO space */
#define VIO_SHIFT       3		/* x = IO space shrink power */
#define PIO_START       0x80000000	/* physical start of IO space */

#define io_p2v( x )             \
   ( (((x)&0x00ffffff) | (((x)&0x30000000)>>VIO_SHIFT)) + VIO_BASE )
#define io_v2p( x )             \
   ( (((x)&0x00ffffff) | (((x)&(0x30000000>>VIO_SHIFT))<<VIO_SHIFT)) + PIO_START )

#ifndef __ASSEMBLY__
#include <asm/types.h>

#if 0
# define __REG(x)	(*((volatile u32 *)io_p2v(x)))
#else
/*
 * This __REG() version gives the same results as the one above,  except
 * that we are fooling gcc somehow so it generates far better and smaller
 * assembly code for access to contigous registers.  It's a shame that gcc
 * doesn't guess this by itself.
 */
typedef struct { volatile u32 offset[4096]; } __regbase;
# define __REGP(x)	((__regbase *)((x)&~4095))->offset[((x)&4095)>>2]
# define __REG(x)	__REGP(io_p2v(x))
#endif

# define __PREG(x)	(io_v2p((u32)&(x)))

#else

# define __REG(x)	io_p2v(x)
# define __PREG(x)	io_v2p(x)

#endif

#include "SA-1100.h"


/*
 * SA1100 GPIO edge detection for IRQs:
 * IRQs are generated on Falling-Edge, Rising-Edge, or both.
 * This must be called *before* the corresponding IRQ is registered.
 * Use this instead of directly setting GRER/GFER.
 */
#define GPIO_NO_EDGES		0
#define GPIO_FALLING_EDGE	1
#define GPIO_RISING_EDGE	2
#define GPIO_BOTH_EDGES 	3
#ifndef __ASSEMBLY__
extern void set_GPIO_IRQ_edge( int gpio_mask, int edge_mask );
#endif


/*
 * Implementation specifics.
 *
 *                      *** BIG FAT NOTE ***
 *
 * Any definitions in these files should be prefixed by an identifier -
 * eg, ASSABET_UCB1300_IRQ  This will allow us to eleminate these
 * ifdefs, and lots of other preprocessor gunk elsewhere.
 *
 * Also, please try to add your entry in alphabetical order.  The
 * initial ones below are the start of the alphabetical list.
 *
 * Do NOT add your ifdefs around your file.
 */

#include "badge4.h"

#include "frodo.h"

#include "h3600.h"

#include "system3.h"

#ifdef CONFIG_SA1100_PANGOLIN
#include "pangolin.h"
#endif

#ifdef CONFIG_SA1100_HUW_WEBPANEL
#include "huw_webpanel.h"
#endif

#ifdef CONFIG_SA1100_PFS168
#include "pfs168.h"
#endif


#ifdef CONFIG_SA1100_YOPY
#include "yopy.h"
#endif

#ifdef CONFIG_SA1100_FREEBIRD
#include "freebird.h"
#endif

#ifdef CONFIG_SA1100_CERF
#include "cerf.h"
#endif

#ifdef CONFIG_SA1100_EMPEG
#include "empeg.h"
#endif

#ifdef CONFIG_SA1100_ITSY
#include "itsy.h"
#endif

#if defined(CONFIG_SA1100_GRAPHICSCLIENT)
#include "graphicsclient.h"
#endif

#if defined(CONFIG_SA1100_OMNIMETER)
#include "omnimeter.h"
#endif

#if defined(CONFIG_SA1100_JORNADA720)
#include "jornada720.h"
#endif

#if defined(CONFIG_SA1100_PLEB)
#include "pleb.h"
#endif

#if defined(CONFIG_SA1100_LART)
#include "lart.h"
#endif

#ifdef CONFIG_SA1100_SIMPAD
#include "simpad.h"
#endif

#ifdef CONFIG_SA1100_SIMPUTER
#include "simputer.h"
#endif

#if defined(CONFIG_SA1100_GRAPHICSMASTER)
#include "graphicsmaster.h"
#endif

#if defined(CONFIG_SA1100_ADSAGC)
#include "adsagc.h"
#endif

#if defined(CONFIG_SA1100_ADSBITSY)
#include "adsbitsy.h"
#endif

#if defined(CONFIG_SA1100_ADSBITSYPLUS)
#include "adsbitsyplus.h"
#endif

#ifdef CONFIG_SA1101

/*
 * We have mapped the sa1101 depending on the value of SA1101_BASE.
 * It then appears from 0xf4000000.
 */

#define SA1101_p2v( x )         ((x) - SA1101_BASE + 0xf4000000)
#define SA1101_v2p( x )         ((x) - 0xf4000000  + SA1101_BASE)

#include "SA-1101.h"

#endif

#if defined(CONFIG_SA1100_OMNIMETER)
#include "omnimeter.h"
#endif

#if defined(CONFIG_SA1100_JORNADA720)
#include "jornada720.h"
#endif

#if defined(CONFIG_SA1100_FLEXANET)
#include "flexanet.h"
#endif

#endif  /* _ASM_ARCH_HARDWARE_H */
