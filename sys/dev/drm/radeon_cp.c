/* radeon_cp.c -- CP support for Radeon -*- linux-c -*-
 *
 * Copyright 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Kevin E. Martin <martin@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "dev/drm/radeon.h"
#include "dev/drm/drmP.h"
#include "dev/drm/radeon_drm.h"
#include "dev/drm/radeon_drv.h"

#ifdef __linux__
#define __NO_VERSION__
#include <linux/interrupt.h>	/* For task queue support */
#include <linux/delay.h>
#endif /* __linux__ */
#ifdef __FreeBSD__
#include <vm/vm.h>
#include <vm/pmap.h>
#endif /* __FreeBSD__ */

#define RADEON_FIFO_DEBUG	0

#if defined(__alpha__)
# define PCIGART_ENABLED
#else
# undef PCIGART_ENABLED
#endif


/* CP microcode (from ATI) */
static u32 radeon_cp_microcode[][2] = {
	{ 0x21007000, 0000000000 },
	{ 0x20007000, 0000000000 },
	{ 0x000000b4, 0x00000004 },
	{ 0x000000b8, 0x00000004 },
	{ 0x6f5b4d4c, 0000000000 },
	{ 0x4c4c427f, 0000000000 },
	{ 0x5b568a92, 0000000000 },
	{ 0x4ca09c6d, 0000000000 },
	{ 0xad4c4c4c, 0000000000 },
	{ 0x4ce1af3d, 0000000000 },
	{ 0xd8afafaf, 0000000000 },
	{ 0xd64c4cdc, 0000000000 },
	{ 0x4cd10d10, 0000000000 },
	{ 0x000f0000, 0x00000016 },
	{ 0x362f242d, 0000000000 },
	{ 0x00000012, 0x00000004 },
	{ 0x000f0000, 0x00000016 },
	{ 0x362f282d, 0000000000 },
	{ 0x000380e7, 0x00000002 },
	{ 0x04002c97, 0x00000002 },
	{ 0x000f0001, 0x00000016 },
	{ 0x333a3730, 0000000000 },
	{ 0x000077ef, 0x00000002 },
	{ 0x00061000, 0x00000002 },
	{ 0x00000021, 0x0000001a },
	{ 0x00004000, 0x0000001e },
	{ 0x00061000, 0x00000002 },
	{ 0x00000021, 0x0000001a },
	{ 0x00004000, 0x0000001e },
	{ 0x00061000, 0x00000002 },
	{ 0x00000021, 0x0000001a },
	{ 0x00004000, 0x0000001e },
	{ 0x00000017, 0x00000004 },
	{ 0x0003802b, 0x00000002 },
	{ 0x040067e0, 0x00000002 },
	{ 0x00000017, 0x00000004 },
	{ 0x000077e0, 0x00000002 },
	{ 0x00065000, 0x00000002 },
	{ 0x000037e1, 0x00000002 },
	{ 0x040067e1, 0x00000006 },
	{ 0x000077e0, 0x00000002 },
	{ 0x000077e1, 0x00000002 },
	{ 0x000077e1, 0x00000006 },
	{ 0xffffffff, 0000000000 },
	{ 0x10000000, 0000000000 },
	{ 0x0003802b, 0x00000002 },
	{ 0x040067e0, 0x00000006 },
	{ 0x00007675, 0x00000002 },
	{ 0x00007676, 0x00000002 },
	{ 0x00007677, 0x00000002 },
	{ 0x00007678, 0x00000006 },
	{ 0x0003802c, 0x00000002 },
	{ 0x04002676, 0x00000002 },
	{ 0x00007677, 0x00000002 },
	{ 0x00007678, 0x00000006 },
	{ 0x0000002f, 0x00000018 },
	{ 0x0000002f, 0x00000018 },
	{ 0000000000, 0x00000006 },
	{ 0x00000030, 0x00000018 },
	{ 0x00000030, 0x00000018 },
	{ 0000000000, 0x00000006 },
	{ 0x01605000, 0x00000002 },
	{ 0x00065000, 0x00000002 },
	{ 0x00098000, 0x00000002 },
	{ 0x00061000, 0x00000002 },
	{ 0x64c0603e, 0x00000004 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x00080000, 0x00000016 },
	{ 0000000000, 0000000000 },
	{ 0x0400251d, 0x00000002 },
	{ 0x00007580, 0x00000002 },
	{ 0x00067581, 0x00000002 },
	{ 0x04002580, 0x00000002 },
	{ 0x00067581, 0x00000002 },
	{ 0x00000049, 0x00000004 },
	{ 0x00005000, 0000000000 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x00061000, 0x00000002 },
	{ 0x0000750e, 0x00000002 },
	{ 0x00019000, 0x00000002 },
	{ 0x00011055, 0x00000014 },
	{ 0x00000055, 0x00000012 },
	{ 0x0400250f, 0x00000002 },
	{ 0x0000504f, 0x00000004 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x00007565, 0x00000002 },
	{ 0x00007566, 0x00000002 },
	{ 0x00000058, 0x00000004 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x01e655b4, 0x00000002 },
	{ 0x4401b0e4, 0x00000002 },
	{ 0x01c110e4, 0x00000002 },
	{ 0x26667066, 0x00000018 },
	{ 0x040c2565, 0x00000002 },
	{ 0x00000066, 0x00000018 },
	{ 0x04002564, 0x00000002 },
	{ 0x00007566, 0x00000002 },
	{ 0x0000005d, 0x00000004 },
	{ 0x00401069, 0x00000008 },
	{ 0x00101000, 0x00000002 },
	{ 0x000d80ff, 0x00000002 },
	{ 0x0080006c, 0x00000008 },
	{ 0x000f9000, 0x00000002 },
	{ 0x000e00ff, 0x00000002 },
	{ 0000000000, 0x00000006 },
	{ 0x0000008f, 0x00000018 },
	{ 0x0000005b, 0x00000004 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x00007576, 0x00000002 },
	{ 0x00065000, 0x00000002 },
	{ 0x00009000, 0x00000002 },
	{ 0x00041000, 0x00000002 },
	{ 0x0c00350e, 0x00000002 },
	{ 0x00049000, 0x00000002 },
	{ 0x00051000, 0x00000002 },
	{ 0x01e785f8, 0x00000002 },
	{ 0x00200000, 0x00000002 },
	{ 0x0060007e, 0x0000000c },
	{ 0x00007563, 0x00000002 },
	{ 0x006075f0, 0x00000021 },
	{ 0x20007073, 0x00000004 },
	{ 0x00005073, 0x00000004 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x00007576, 0x00000002 },
	{ 0x00007577, 0x00000002 },
	{ 0x0000750e, 0x00000002 },
	{ 0x0000750f, 0x00000002 },
	{ 0x00a05000, 0x00000002 },
	{ 0x00600083, 0x0000000c },
	{ 0x006075f0, 0x00000021 },
	{ 0x000075f8, 0x00000002 },
	{ 0x00000083, 0x00000004 },
	{ 0x000a750e, 0x00000002 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x0020750f, 0x00000002 },
	{ 0x00600086, 0x00000004 },
	{ 0x00007570, 0x00000002 },
	{ 0x00007571, 0x00000002 },
	{ 0x00007572, 0x00000006 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x00005000, 0x00000002 },
	{ 0x00a05000, 0x00000002 },
	{ 0x00007568, 0x00000002 },
	{ 0x00061000, 0x00000002 },
	{ 0x00000095, 0x0000000c },
	{ 0x00058000, 0x00000002 },
	{ 0x0c607562, 0x00000002 },
	{ 0x00000097, 0x00000004 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x00600096, 0x00000004 },
	{ 0x400070e5, 0000000000 },
	{ 0x000380e6, 0x00000002 },
	{ 0x040025c5, 0x00000002 },
	{ 0x000380e5, 0x00000002 },
	{ 0x000000a8, 0x0000001c },
	{ 0x000650aa, 0x00000018 },
	{ 0x040025bb, 0x00000002 },
	{ 0x000610ab, 0x00000018 },
	{ 0x040075bc, 0000000000 },
	{ 0x000075bb, 0x00000002 },
	{ 0x000075bc, 0000000000 },
	{ 0x00090000, 0x00000006 },
	{ 0x00090000, 0x00000002 },
	{ 0x000d8002, 0x00000006 },
	{ 0x00007832, 0x00000002 },
	{ 0x00005000, 0x00000002 },
	{ 0x000380e7, 0x00000002 },
	{ 0x04002c97, 0x00000002 },
	{ 0x00007820, 0x00000002 },
	{ 0x00007821, 0x00000002 },
	{ 0x00007800, 0000000000 },
	{ 0x01200000, 0x00000002 },
	{ 0x20077000, 0x00000002 },
	{ 0x01200000, 0x00000002 },
	{ 0x20007000, 0x00000002 },
	{ 0x00061000, 0x00000002 },
	{ 0x0120751b, 0x00000002 },
	{ 0x8040750a, 0x00000002 },
	{ 0x8040750b, 0x00000002 },
	{ 0x00110000, 0x00000002 },
	{ 0x000380e5, 0x00000002 },
	{ 0x000000c6, 0x0000001c },
	{ 0x000610ab, 0x00000018 },
	{ 0x844075bd, 0x00000002 },
	{ 0x000610aa, 0x00000018 },
	{ 0x840075bb, 0x00000002 },
	{ 0x000610ab, 0x00000018 },
	{ 0x844075bc, 0x00000002 },
	{ 0x000000c9, 0x00000004 },
	{ 0x804075bd, 0x00000002 },
	{ 0x800075bb, 0x00000002 },
	{ 0x804075bc, 0x00000002 },
	{ 0x00108000, 0x00000002 },
	{ 0x01400000, 0x00000002 },
	{ 0x006000cd, 0x0000000c },
	{ 0x20c07000, 0x00000020 },
	{ 0x000000cf, 0x00000012 },
	{ 0x00800000, 0x00000006 },
	{ 0x0080751d, 0x00000006 },
	{ 0000000000, 0000000000 },
	{ 0x0000775c, 0x00000002 },
	{ 0x00a05000, 0x00000002 },
	{ 0x00661000, 0x00000002 },
	{ 0x0460275d, 0x00000020 },
	{ 0x00004000, 0000000000 },
	{ 0x01e00830, 0x00000002 },
	{ 0x21007000, 0000000000 },
	{ 0x6464614d, 0000000000 },
	{ 0x69687420, 0000000000 },
	{ 0x00000073, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0x00005000, 0x00000002 },
	{ 0x000380d0, 0x00000002 },
	{ 0x040025e0, 0x00000002 },
	{ 0x000075e1, 0000000000 },
	{ 0x00000001, 0000000000 },
	{ 0x000380e0, 0x00000002 },
	{ 0x04002394, 0x00000002 },
	{ 0x00005000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0x00000008, 0000000000 },
	{ 0x00000004, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
	{ 0000000000, 0000000000 },
};


int RADEON_READ_PLL(drm_device_t *dev, int addr)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	RADEON_WRITE8(RADEON_CLOCK_CNTL_INDEX, addr & 0x1f);
	return RADEON_READ(RADEON_CLOCK_CNTL_DATA);
}

#if RADEON_FIFO_DEBUG
static void radeon_status( drm_radeon_private_t *dev_priv )
{
	printk( "%s:\n", __FUNCTION__ );
	printk( "RBBM_STATUS = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_RBBM_STATUS ) );
	printk( "CP_RB_RTPR = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_CP_RB_RPTR ) );
	printk( "CP_RB_WTPR = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_CP_RB_WPTR ) );
	printk( "AIC_CNTL = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_AIC_CNTL ) );
	printk( "AIC_STAT = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_AIC_STAT ) );
	printk( "AIC_PT_BASE = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_AIC_PT_BASE ) );
	printk( "TLB_ADDR = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_AIC_TLB_ADDR ) );
	printk( "TLB_DATA = 0x%08x\n",
		(unsigned int)RADEON_READ( RADEON_AIC_TLB_DATA ) );
}
#endif


/* ================================================================
 * Engine, FIFO control
 */

static int radeon_do_pixcache_flush( drm_radeon_private_t *dev_priv )
{
	u32 tmp;
	int i;

	tmp  = RADEON_READ( RADEON_RB2D_DSTCACHE_CTLSTAT );
	tmp |= RADEON_RB2D_DC_FLUSH_ALL;
	RADEON_WRITE( RADEON_RB2D_DSTCACHE_CTLSTAT, tmp );

	for ( i = 0 ; i < dev_priv->usec_timeout ; i++ ) {
		if ( !(RADEON_READ( RADEON_RB2D_DSTCACHE_CTLSTAT )
		       & RADEON_RB2D_DC_BUSY) ) {
			return 0;
		}
		DRM_OS_DELAY( 1 );
	}

#if RADEON_FIFO_DEBUG
	DRM_ERROR( "failed!\n" );
	radeon_status( dev_priv );
#endif
	return DRM_OS_ERR(EBUSY);
}

static int radeon_do_wait_for_fifo( drm_radeon_private_t *dev_priv,
				    int entries )
{
	int i;

	for ( i = 0 ; i < dev_priv->usec_timeout ; i++ ) {
		int slots = ( RADEON_READ( RADEON_RBBM_STATUS )
			      & RADEON_RBBM_FIFOCNT_MASK );
		if ( slots >= entries ) return 0;
		DRM_OS_DELAY( 1 );
	}

#if RADEON_FIFO_DEBUG
	DRM_ERROR( "failed!\n" );
	radeon_status( dev_priv );
#endif
	return DRM_OS_ERR(EBUSY);
}

static int radeon_do_wait_for_idle( drm_radeon_private_t *dev_priv )
{
	int i, ret;

	ret = radeon_do_wait_for_fifo( dev_priv, 64 );
	if ( ret ) return ret;
	for ( i = 0 ; i < dev_priv->usec_timeout ; i++ ) {
		if ( !(RADEON_READ( RADEON_RBBM_STATUS )
		       & RADEON_RBBM_ACTIVE) ) {
			radeon_do_pixcache_flush( dev_priv );
			return 0;
		}
		DRM_OS_DELAY( 1 );
	}

#if RADEON_FIFO_DEBUG
	DRM_ERROR( "failed!\n" );
	radeon_status( dev_priv );
#endif
	return DRM_OS_ERR(EBUSY);
}


/* ================================================================
 * CP control, initialization
 */

/* Load the microcode for the CP */
static void radeon_cp_load_microcode( drm_radeon_private_t *dev_priv )
{
	int i;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	radeon_do_wait_for_idle( dev_priv );

	RADEON_WRITE( RADEON_CP_ME_RAM_ADDR, 0 );
	for ( i = 0 ; i < 256 ; i++ ) {
		RADEON_WRITE( RADEON_CP_ME_RAM_DATAH,
			      radeon_cp_microcode[i][1] );
		RADEON_WRITE( RADEON_CP_ME_RAM_DATAL,
			      radeon_cp_microcode[i][0] );
	}
}

/* Flush any pending commands to the CP.  This should only be used just
 * prior to a wait for idle, as it informs the engine that the command
 * stream is ending.
 */
static void radeon_do_cp_flush( drm_radeon_private_t *dev_priv )
{
	DRM_DEBUG( "%s\n", __FUNCTION__ );
#if 0
	u32 tmp;

	tmp = RADEON_READ( RADEON_CP_RB_WPTR ) | (1 << 31);
	RADEON_WRITE( RADEON_CP_RB_WPTR, tmp );
#endif
}

/* Wait for the CP to go idle.
 */
int radeon_do_cp_idle( drm_radeon_private_t *dev_priv )
{
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	BEGIN_RING( 6 );

	RADEON_PURGE_CACHE();
	RADEON_PURGE_ZCACHE();
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();

	return radeon_do_wait_for_idle( dev_priv );
}

/* Start the Command Processor.
 */
static void radeon_do_cp_start( drm_radeon_private_t *dev_priv )
{
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	radeon_do_wait_for_idle( dev_priv );

	RADEON_WRITE( RADEON_CP_CSQ_CNTL, dev_priv->cp_mode );

	dev_priv->cp_running = 1;

	BEGIN_RING( 6 );

	RADEON_PURGE_CACHE();
	RADEON_PURGE_ZCACHE();
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();
}

/* Reset the Command Processor.  This will not flush any pending
 * commands, so you must wait for the CP command stream to complete
 * before calling this routine.
 */
static void radeon_do_cp_reset( drm_radeon_private_t *dev_priv )
{
	u32 cur_read_ptr;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	cur_read_ptr = RADEON_READ( RADEON_CP_RB_RPTR );
	RADEON_WRITE( RADEON_CP_RB_WPTR, cur_read_ptr );
	*dev_priv->ring.head = cur_read_ptr;
	dev_priv->ring.tail = cur_read_ptr;
}

/* Stop the Command Processor.  This will not flush any pending
 * commands, so you must flush the command stream and wait for the CP
 * to go idle before calling this routine.
 */
static void radeon_do_cp_stop( drm_radeon_private_t *dev_priv )
{
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	RADEON_WRITE( RADEON_CP_CSQ_CNTL, RADEON_CSQ_PRIDIS_INDDIS );

	dev_priv->cp_running = 0;
}

/* Reset the engine.  This will stop the CP if it is running.
 */
static int radeon_do_engine_reset( drm_device_t *dev )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	u32 clock_cntl_index, mclk_cntl, rbbm_soft_reset;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	radeon_do_pixcache_flush( dev_priv );

	clock_cntl_index = RADEON_READ( RADEON_CLOCK_CNTL_INDEX );
	mclk_cntl = RADEON_READ_PLL( dev, RADEON_MCLK_CNTL );

	RADEON_WRITE_PLL( RADEON_MCLK_CNTL, ( mclk_cntl |
					      RADEON_FORCEON_MCLKA |
					      RADEON_FORCEON_MCLKB |
 					      RADEON_FORCEON_YCLKA |
					      RADEON_FORCEON_YCLKB |
					      RADEON_FORCEON_MC |
					      RADEON_FORCEON_AIC ) );

	rbbm_soft_reset = RADEON_READ( RADEON_RBBM_SOFT_RESET );

	RADEON_WRITE( RADEON_RBBM_SOFT_RESET, ( rbbm_soft_reset |
						RADEON_SOFT_RESET_CP |
						RADEON_SOFT_RESET_HI |
						RADEON_SOFT_RESET_SE |
						RADEON_SOFT_RESET_RE |
						RADEON_SOFT_RESET_PP |
						RADEON_SOFT_RESET_E2 |
						RADEON_SOFT_RESET_RB ) );
	RADEON_READ( RADEON_RBBM_SOFT_RESET );
	RADEON_WRITE( RADEON_RBBM_SOFT_RESET, ( rbbm_soft_reset &
						~( RADEON_SOFT_RESET_CP |
						   RADEON_SOFT_RESET_HI |
						   RADEON_SOFT_RESET_SE |
						   RADEON_SOFT_RESET_RE |
						   RADEON_SOFT_RESET_PP |
						   RADEON_SOFT_RESET_E2 |
						   RADEON_SOFT_RESET_RB ) ) );
	RADEON_READ( RADEON_RBBM_SOFT_RESET );


	RADEON_WRITE_PLL( RADEON_MCLK_CNTL, mclk_cntl );
	RADEON_WRITE( RADEON_CLOCK_CNTL_INDEX, clock_cntl_index );
	RADEON_WRITE( RADEON_RBBM_SOFT_RESET,  rbbm_soft_reset );

	/* Reset the CP ring */
	radeon_do_cp_reset( dev_priv );

	/* The CP is no longer running after an engine reset */
	dev_priv->cp_running = 0;

	/* Reset any pending vertex, indirect buffers */
	radeon_freelist_reset( dev );

	return 0;
}

static void radeon_cp_init_ring_buffer( drm_device_t *dev,
				        drm_radeon_private_t *dev_priv )
{
	u32 ring_start, cur_read_ptr;
	u32 tmp;

	/* Initialize the memory controller */
	RADEON_WRITE( RADEON_MC_FB_LOCATION,
		      (dev_priv->agp_vm_start - 1) & 0xffff0000 );

	if ( !dev_priv->is_pci ) {
		RADEON_WRITE( RADEON_MC_AGP_LOCATION,
			      (((dev_priv->agp_vm_start - 1 +
				 dev_priv->agp_size) & 0xffff0000) |
			       (dev_priv->agp_vm_start >> 16)) );
	}

#if __REALLY_HAVE_AGP
	if ( !dev_priv->is_pci )
		ring_start = (dev_priv->cp_ring->offset
			      - dev->agp->base
			      + dev_priv->agp_vm_start);
       else
#endif
		ring_start = (dev_priv->cp_ring->offset
			      - dev->sg->handle
			      + dev_priv->agp_vm_start);

	RADEON_WRITE( RADEON_CP_RB_BASE, ring_start );

	/* Set the write pointer delay */
	RADEON_WRITE( RADEON_CP_RB_WPTR_DELAY, 0 );

	/* Initialize the ring buffer's read and write pointers */
	cur_read_ptr = RADEON_READ( RADEON_CP_RB_RPTR );
	RADEON_WRITE( RADEON_CP_RB_WPTR, cur_read_ptr );
	*dev_priv->ring.head = cur_read_ptr;
	dev_priv->ring.tail = cur_read_ptr;

#if __REALLY_HAVE_SG
	if ( !dev_priv->is_pci ) {
#endif
		RADEON_WRITE( RADEON_CP_RB_RPTR_ADDR,
			      dev_priv->ring_rptr->offset );
#if __REALLY_HAVE_SG
	} else {
		drm_sg_mem_t *entry = dev->sg;
		unsigned long tmp_ofs, page_ofs;

		tmp_ofs = dev_priv->ring_rptr->offset - dev->sg->handle;
		page_ofs = tmp_ofs >> PAGE_SHIFT;

		RADEON_WRITE( RADEON_CP_RB_RPTR_ADDR,
			     entry->busaddr[page_ofs]);
		DRM_DEBUG( "ring rptr: offset=0x%08x handle=0x%08lx\n",
			   entry->busaddr[page_ofs],
			   entry->handle + tmp_ofs );
	}
#endif

	/* Set ring buffer size */
	RADEON_WRITE( RADEON_CP_RB_CNTL, dev_priv->ring.size_l2qw );

	radeon_do_wait_for_idle( dev_priv );

	/* Turn on bus mastering */
	tmp = RADEON_READ( RADEON_BUS_CNTL ) & ~RADEON_BUS_MASTER_DIS;
	RADEON_WRITE( RADEON_BUS_CNTL, tmp );

	/* Sync everything up */
	RADEON_WRITE( RADEON_ISYNC_CNTL,
		      (RADEON_ISYNC_ANY2D_IDLE3D |
		       RADEON_ISYNC_ANY3D_IDLE2D |
		       RADEON_ISYNC_WAIT_IDLEGUI |
		       RADEON_ISYNC_CPSCRATCH_IDLEGUI) );
}

static int radeon_do_init_cp( drm_device_t *dev, drm_radeon_init_t *init )
{
	drm_radeon_private_t *dev_priv;
#ifdef __linux__
	struct list_head *list;
#endif /* __linux__ */
#ifdef __FreeBSD__
	drm_map_list_entry_t *listentry;
#endif /* __FreeBSD__ */
	u32 tmp;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	dev_priv = DRM(alloc)( sizeof(drm_radeon_private_t), DRM_MEM_DRIVER );
	if ( dev_priv == NULL )
		return DRM_OS_ERR(ENOMEM);

	memset( dev_priv, 0, sizeof(drm_radeon_private_t) );

	dev_priv->is_pci = init->is_pci;

#if !defined(PCIGART_ENABLED)
	/* PCI support is not 100% working, so we disable it here.
	 */
	if ( dev_priv->is_pci ) {
		DRM_ERROR( "PCI GART not yet supported for Radeon!\n" );
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}
#endif

	if ( dev_priv->is_pci && !dev->sg ) {
		DRM_ERROR( "PCI GART memory not allocated!\n" );
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}

	dev_priv->usec_timeout = init->usec_timeout;
	if ( dev_priv->usec_timeout < 1 ||
	     dev_priv->usec_timeout > RADEON_MAX_USEC_TIMEOUT ) {
		DRM_DEBUG( "TIMEOUT problem!\n" );
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}

	dev_priv->cp_mode = init->cp_mode;

	/* Simple idle check.
	 */
	atomic_set( &dev_priv->idle_count, 0 );

	/* We don't support anything other than bus-mastering ring mode,
	 * but the ring can be in either AGP or PCI space for the ring
	 * read pointer.
	 */
	if ( ( init->cp_mode != RADEON_CSQ_PRIBM_INDDIS ) &&
	     ( init->cp_mode != RADEON_CSQ_PRIBM_INDBM ) ) {
		DRM_DEBUG( "BAD cp_mode (%x)!\n", init->cp_mode );
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}

	switch ( init->fb_bpp ) {
	case 16:
		dev_priv->color_fmt = RADEON_COLOR_FORMAT_RGB565;
		break;
	case 32:
	default:
		dev_priv->color_fmt = RADEON_COLOR_FORMAT_ARGB8888;
		break;
	}
	dev_priv->front_offset	= init->front_offset;
	dev_priv->front_pitch	= init->front_pitch;
	dev_priv->back_offset	= init->back_offset;
	dev_priv->back_pitch	= init->back_pitch;

	switch ( init->depth_bpp ) {
	case 16:
		dev_priv->depth_fmt = RADEON_DEPTH_FORMAT_16BIT_INT_Z;
		break;
	case 32:
	default:
		dev_priv->depth_fmt = RADEON_DEPTH_FORMAT_24BIT_INT_Z;
		break;
	}
	dev_priv->depth_offset	= init->depth_offset;
	dev_priv->depth_pitch	= init->depth_pitch;

	dev_priv->front_pitch_offset = (((dev_priv->front_pitch/64) << 22) |
					(dev_priv->front_offset >> 10));
	dev_priv->back_pitch_offset = (((dev_priv->back_pitch/64) << 22) |
				       (dev_priv->back_offset >> 10));
	dev_priv->depth_pitch_offset = (((dev_priv->depth_pitch/64) << 22) |
					(dev_priv->depth_offset >> 10));

	/* Hardware state for depth clears.  Remove this if/when we no
	 * longer clear the depth buffer with a 3D rectangle.  Hard-code
	 * all values to prevent unwanted 3D state from slipping through
	 * and screwing with the clear operation.
	 */
	dev_priv->depth_clear.rb3d_cntl = (RADEON_PLANE_MASK_ENABLE |
					   RADEON_Z_ENABLE |
					   (dev_priv->color_fmt << 10) |
					   RADEON_ZBLOCK16);

	dev_priv->depth_clear.rb3d_zstencilcntl = (dev_priv->depth_fmt |
						   RADEON_Z_TEST_ALWAYS |
						   RADEON_STENCIL_TEST_ALWAYS |
						   RADEON_STENCIL_S_FAIL_KEEP |
						   RADEON_STENCIL_ZPASS_KEEP |
						   RADEON_STENCIL_ZFAIL_KEEP |
						   RADEON_Z_WRITE_ENABLE);

	dev_priv->depth_clear.se_cntl = (RADEON_FFACE_CULL_CW |
					 RADEON_BFACE_SOLID |
					 RADEON_FFACE_SOLID |
					 RADEON_FLAT_SHADE_VTX_LAST |
					 RADEON_DIFFUSE_SHADE_FLAT |
					 RADEON_ALPHA_SHADE_FLAT |
					 RADEON_SPECULAR_SHADE_FLAT |
					 RADEON_FOG_SHADE_FLAT |
					 RADEON_VTX_PIX_CENTER_OGL |
					 RADEON_ROUND_MODE_TRUNC |
					 RADEON_ROUND_PREC_8TH_PIX);

#ifdef __linux__
	list_for_each(list, &dev->maplist->head) {
		drm_map_list_t *r_list = (drm_map_list_t *)list;
		if( r_list->map &&
		    r_list->map->type == _DRM_SHM &&
		    r_list->map->flags & _DRM_CONTAINS_LOCK ) {
			dev_priv->sarea = r_list->map;
 			break;
 		}
 	}
#endif /* __linux__ */
#ifdef __FreeBSD__
	TAILQ_FOREACH(listentry, dev->maplist, link) {
		drm_map_t *map = listentry->map;
		if (map->type == _DRM_SHM &&
			map->flags & _DRM_CONTAINS_LOCK) {
			dev_priv->sarea = map;
			break;
		}
	}
#endif /* __FreeBSD__ */

	if(!dev_priv->sarea) {
		DRM_ERROR("could not find sarea!\n");
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}

	DRM_FIND_MAP( dev_priv->fb, init->fb_offset );
	if(!dev_priv->fb) {
		DRM_ERROR("could not find framebuffer!\n");
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}
	DRM_FIND_MAP( dev_priv->mmio, init->mmio_offset );
	if(!dev_priv->mmio) {
		DRM_ERROR("could not find mmio region!\n");
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}
	DRM_FIND_MAP( dev_priv->cp_ring, init->ring_offset );
	if(!dev_priv->cp_ring) {
		DRM_ERROR("could not find cp ring region!\n");
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}
	DRM_FIND_MAP( dev_priv->ring_rptr, init->ring_rptr_offset );
	if(!dev_priv->ring_rptr) {
		DRM_ERROR("could not find ring read pointer!\n");
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}
	DRM_FIND_MAP( dev_priv->buffers, init->buffers_offset );
	if(!dev_priv->buffers) {
		DRM_ERROR("could not find dma buffer region!\n");
		dev->dev_private = (void *)dev_priv;
		radeon_do_cleanup_cp(dev);
		return DRM_OS_ERR(EINVAL);
	}

	if ( !dev_priv->is_pci ) {
		DRM_FIND_MAP( dev_priv->agp_textures,
			      init->agp_textures_offset );
		if(!dev_priv->agp_textures) {
			DRM_ERROR("could not find agp texture region!\n");
			dev->dev_private = (void *)dev_priv;
			radeon_do_cleanup_cp(dev);
			return DRM_OS_ERR(EINVAL);
		}
	}

	dev_priv->sarea_priv =
		(drm_radeon_sarea_t *)((u8 *)dev_priv->sarea->handle +
				       init->sarea_priv_offset);

	if ( !dev_priv->is_pci ) {
		DRM_IOREMAP( dev_priv->cp_ring );
		DRM_IOREMAP( dev_priv->ring_rptr );
		DRM_IOREMAP( dev_priv->buffers );
		if(!dev_priv->cp_ring->handle ||
		   !dev_priv->ring_rptr->handle ||
		   !dev_priv->buffers->handle) {
			DRM_ERROR("could not find ioremap agp regions!\n");
			dev->dev_private = (void *)dev_priv;
			radeon_do_cleanup_cp(dev);
			return DRM_OS_ERR(EINVAL);
		}
	} else {
		dev_priv->cp_ring->handle =
			(void *)dev_priv->cp_ring->offset;
		dev_priv->ring_rptr->handle =
			(void *)dev_priv->ring_rptr->offset;
		dev_priv->buffers->handle = (void *)dev_priv->buffers->offset;

		DRM_DEBUG( "dev_priv->cp_ring->handle %p\n",
			   dev_priv->cp_ring->handle );
		DRM_DEBUG( "dev_priv->ring_rptr->handle %p\n",
			   dev_priv->ring_rptr->handle );
		DRM_DEBUG( "dev_priv->buffers->handle %p\n",
			   dev_priv->buffers->handle );
	}


	dev_priv->agp_size = init->agp_size;
	dev_priv->agp_vm_start = RADEON_READ( RADEON_CONFIG_APER_SIZE );
#if __REALLY_HAVE_AGP
	if ( !dev_priv->is_pci )
		dev_priv->agp_buffers_offset = (dev_priv->buffers->offset
						- dev->agp->base
						+ dev_priv->agp_vm_start);
	else
#endif
		dev_priv->agp_buffers_offset = (dev_priv->buffers->offset
						- dev->sg->handle
						+ dev_priv->agp_vm_start);

	DRM_DEBUG( "dev_priv->agp_size %d\n",
		   dev_priv->agp_size );
	DRM_DEBUG( "dev_priv->agp_vm_start 0x%x\n",
		   dev_priv->agp_vm_start );
	DRM_DEBUG( "dev_priv->agp_buffers_offset 0x%lx\n",
		   dev_priv->agp_buffers_offset );

	dev_priv->ring.head = ((__volatile__ u32 *)
			       dev_priv->ring_rptr->handle);

	dev_priv->ring.start = (u32 *)dev_priv->cp_ring->handle;
	dev_priv->ring.end = ((u32 *)dev_priv->cp_ring->handle
			      + init->ring_size / sizeof(u32));
	dev_priv->ring.size = init->ring_size;
	dev_priv->ring.size_l2qw = DRM(order)( init->ring_size / 8 );

	dev_priv->ring.tail_mask =
		(dev_priv->ring.size / sizeof(u32)) - 1;

	dev_priv->ring.high_mark = RADEON_RING_HIGH_MARK;

#if 0
	/* Initialize the scratch register pointer.  This will cause
	 * the scratch register values to be written out to memory
	 * whenever they are updated.
	 * FIXME: This doesn't quite work yet, so we're disabling it
	 * for the release.
	 */
	RADEON_WRITE( RADEON_SCRATCH_ADDR, (dev_priv->ring_rptr->offset +
					    RADEON_SCRATCH_REG_OFFSET) );
	RADEON_WRITE( RADEON_SCRATCH_UMSK, 0x7 );
#endif

	dev_priv->scratch = ((__volatile__ u32 *)
			     dev_priv->ring_rptr->handle +
			     (RADEON_SCRATCH_REG_OFFSET / sizeof(u32)));

	dev_priv->sarea_priv->last_frame = 0;
	RADEON_WRITE( RADEON_LAST_FRAME_REG,
		      dev_priv->sarea_priv->last_frame );

	dev_priv->sarea_priv->last_dispatch = 0;
	RADEON_WRITE( RADEON_LAST_DISPATCH_REG,
		      dev_priv->sarea_priv->last_dispatch );

	dev_priv->sarea_priv->last_clear = 0;
	RADEON_WRITE( RADEON_LAST_CLEAR_REG,
		      dev_priv->sarea_priv->last_clear );

#if __REALLY_HAVE_SG
	if ( dev_priv->is_pci ) {
		if (!DRM(ati_pcigart_init)( dev, &dev_priv->phys_pci_gart,
					    &dev_priv->bus_pci_gart)) {
			DRM_ERROR( "failed to init PCI GART!\n" );
			dev->dev_private = (void *)dev_priv;
			radeon_do_cleanup_cp(dev);
			return DRM_OS_ERR(ENOMEM);
		}
		/* Turn on PCI GART
		 */
		tmp = RADEON_READ( RADEON_AIC_CNTL )
		      | RADEON_PCIGART_TRANSLATE_EN;
		RADEON_WRITE( RADEON_AIC_CNTL, tmp );

		/* set PCI GART page-table base address
		 */
		RADEON_WRITE( RADEON_AIC_PT_BASE, dev_priv->bus_pci_gart );

		/* set address range for PCI address translate
		 */
		RADEON_WRITE( RADEON_AIC_LO_ADDR, dev_priv->agp_vm_start );
		RADEON_WRITE( RADEON_AIC_HI_ADDR, dev_priv->agp_vm_start
						  + dev_priv->agp_size - 1);

		/* Turn off AGP aperture -- is this required for PCIGART?
		 */
		RADEON_WRITE( RADEON_MC_AGP_LOCATION, 0xffffffc0 ); /* ?? */
		RADEON_WRITE( RADEON_AGP_COMMAND, 0 ); /* clear AGP_COMMAND */
	} else {
#endif
		/* Turn off PCI GART
		 */
		tmp = RADEON_READ( RADEON_AIC_CNTL )
		      & ~RADEON_PCIGART_TRANSLATE_EN;
		RADEON_WRITE( RADEON_AIC_CNTL, tmp );
#if __REALLY_HAVE_SG
	}
#endif

	radeon_cp_load_microcode( dev_priv );
	radeon_cp_init_ring_buffer( dev, dev_priv );

#if ROTATE_BUFS
	dev_priv->last_buf = 0;
#endif

	dev->dev_private = (void *)dev_priv;

	radeon_do_engine_reset( dev );

	return 0;
}

int radeon_do_cleanup_cp( drm_device_t *dev )
{
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	if ( dev->dev_private ) {
		drm_radeon_private_t *dev_priv = dev->dev_private;

#if __REALLY_HAVE_SG
		if ( !dev_priv->is_pci ) {
#endif
			DRM_IOREMAPFREE( dev_priv->cp_ring );
			DRM_IOREMAPFREE( dev_priv->ring_rptr );
			DRM_IOREMAPFREE( dev_priv->buffers );
#if __REALLY_HAVE_SG
		} else {
			if (!DRM(ati_pcigart_cleanup)( dev,
						dev_priv->phys_pci_gart,
						dev_priv->bus_pci_gart ))
				DRM_ERROR( "failed to cleanup PCI GART!\n" );
		}
#endif

		DRM(free)( dev->dev_private, sizeof(drm_radeon_private_t),
			   DRM_MEM_DRIVER );
		dev->dev_private = NULL;
	}

	return 0;
}

int radeon_cp_init( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_radeon_init_t init;

	DRM_OS_KRNFROMUSR( init, (drm_radeon_init_t *) data, sizeof(init) );

	switch ( init.func ) {
	case RADEON_INIT_CP:
		return radeon_do_init_cp( dev, &init );
	case RADEON_CLEANUP_CP:
		return radeon_do_cleanup_cp( dev );
	}

	return DRM_OS_ERR(EINVAL);
}

int radeon_cp_start( DRM_OS_IOCTL )
{
 	DRM_OS_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	LOCK_TEST_WITH_RETURN( dev );

	if ( dev_priv->cp_running ) {
		DRM_DEBUG( "%s while CP running\n", __FUNCTION__ );
		return 0;
	}
	if ( dev_priv->cp_mode == RADEON_CSQ_PRIDIS_INDDIS ) {
		DRM_DEBUG( "%s called with bogus CP mode (%d)\n",
			   __FUNCTION__, dev_priv->cp_mode );
		return 0;
	}

	radeon_do_cp_start( dev_priv );

	return 0;
}

/* Stop the CP.  The engine must have been idled before calling this
 * routine.
 */
int radeon_cp_stop( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_cp_stop_t stop;
	int ret;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	LOCK_TEST_WITH_RETURN( dev );

	DRM_OS_KRNFROMUSR( stop, (drm_radeon_cp_stop_t *) data, sizeof(stop) );

	/* Flush any pending CP commands.  This ensures any outstanding
	 * commands are exectuted by the engine before we turn it off.
	 */
	if ( stop.flush ) {
		radeon_do_cp_flush( dev_priv );
	}

	/* If we fail to make the engine go idle, we return an error
	 * code so that the DRM ioctl wrapper can try again.
	 */
	if ( stop.idle ) {
		ret = radeon_do_cp_idle( dev_priv );
		if ( ret ) return ret;
	}

	/* Finally, we can turn off the CP.  If the engine isn't idle,
	 * we will get some dropped triangles as they won't be fully
	 * rendered before the CP is shut down.
	 */
	radeon_do_cp_stop( dev_priv );

	/* Reset the engine */
	radeon_do_engine_reset( dev );

	return 0;
}

/* Just reset the CP ring.  Called as part of an X Server engine reset.
 */
int radeon_cp_reset( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	LOCK_TEST_WITH_RETURN( dev );

	if ( !dev_priv ) {
		DRM_DEBUG( "%s called before init done\n", __FUNCTION__ );
		return DRM_OS_ERR(EINVAL);
	}

	radeon_do_cp_reset( dev_priv );

	/* The CP is no longer running after an engine reset */
	dev_priv->cp_running = 0;

	return 0;
}

int radeon_cp_idle( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	LOCK_TEST_WITH_RETURN( dev );

	return radeon_do_cp_idle( dev_priv );
}

int radeon_engine_reset( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	LOCK_TEST_WITH_RETURN( dev );

	return radeon_do_engine_reset( dev );
}


/* ================================================================
 * Fullscreen mode
 */

static int radeon_do_init_pageflip( drm_device_t *dev )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	dev_priv->crtc_offset =      RADEON_READ( RADEON_CRTC_OFFSET );
	dev_priv->crtc_offset_cntl = RADEON_READ( RADEON_CRTC_OFFSET_CNTL );

	RADEON_WRITE( RADEON_CRTC_OFFSET, dev_priv->front_offset );
	RADEON_WRITE( RADEON_CRTC_OFFSET_CNTL,
		      dev_priv->crtc_offset_cntl |
		      RADEON_CRTC_OFFSET_FLIP_CNTL );

	dev_priv->page_flipping = 1;
	dev_priv->current_page = 0;

	return 0;
}

int radeon_do_cleanup_pageflip( drm_device_t *dev )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	RADEON_WRITE( RADEON_CRTC_OFFSET,      dev_priv->crtc_offset );
	RADEON_WRITE( RADEON_CRTC_OFFSET_CNTL, dev_priv->crtc_offset_cntl );

	dev_priv->page_flipping = 0;
	dev_priv->current_page = 0;

	return 0;
}

int radeon_fullscreen( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_radeon_fullscreen_t fs;

	LOCK_TEST_WITH_RETURN( dev );

	DRM_OS_KRNFROMUSR( fs, (drm_radeon_fullscreen_t *) data,
			     sizeof(fs) );

	switch ( fs.func ) {
	case RADEON_INIT_FULLSCREEN:
		return radeon_do_init_pageflip( dev );
	case RADEON_CLEANUP_FULLSCREEN:
		return radeon_do_cleanup_pageflip( dev );
	}

	return DRM_OS_ERR(EINVAL);
}


/* ================================================================
 * Freelist management
 */
#define RADEON_BUFFER_USED	0xffffffff
#define RADEON_BUFFER_FREE	0

#if 0
static int radeon_freelist_init( drm_device_t *dev )
{
	drm_device_dma_t *dma = dev->dma;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_freelist_t *entry;
	int i;

	dev_priv->head = DRM(alloc)( sizeof(drm_radeon_freelist_t),
				     DRM_MEM_DRIVER );
	if ( dev_priv->head == NULL )
		return DRM_OS_ERR(ENOMEM);

	memset( dev_priv->head, 0, sizeof(drm_radeon_freelist_t) );
	dev_priv->head->age = RADEON_BUFFER_USED;

	for ( i = 0 ; i < dma->buf_count ; i++ ) {
		buf = dma->buflist[i];
		buf_priv = buf->dev_private;

		entry = DRM(alloc)( sizeof(drm_radeon_freelist_t),
				    DRM_MEM_DRIVER );
		if ( !entry ) return DRM_OS_ERR(ENOMEM);

		entry->age = RADEON_BUFFER_FREE;
		entry->buf = buf;
		entry->prev = dev_priv->head;
		entry->next = dev_priv->head->next;
		if ( !entry->next )
			dev_priv->tail = entry;

		buf_priv->discard = 0;
		buf_priv->dispatched = 0;
		buf_priv->list_entry = entry;

		dev_priv->head->next = entry;

		if ( dev_priv->head->next )
			dev_priv->head->next->prev = entry;
	}

	return 0;

}
#endif

drm_buf_t *radeon_freelist_get( drm_device_t *dev )
{
	drm_device_dma_t *dma = dev->dma;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv;
	drm_buf_t *buf;
	int i, t;
#if ROTATE_BUFS
	int start;
#endif

	/* FIXME: Optimize -- use freelist code */

	for ( i = 0 ; i < dma->buf_count ; i++ ) {
		buf = dma->buflist[i];
		buf_priv = buf->dev_private;
		if ( buf->pid == 0 ) {
			DRM_DEBUG( "  ret buf=%d last=%d pid=0\n",
				   buf->idx, dev_priv->last_buf );
			return buf;
		}
		DRM_DEBUG( "    skipping buf=%d pid=%d\n",
			   buf->idx, buf->pid );
	}

#if ROTATE_BUFS
	if ( ++dev_priv->last_buf >= dma->buf_count )
		dev_priv->last_buf = 0;
	start = dev_priv->last_buf;
#endif
	for ( t = 0 ; t < dev_priv->usec_timeout ; t++ ) {
#if 0
		/* FIXME: Disable this for now */
		u32 done_age = dev_priv->scratch[RADEON_LAST_DISPATCH];
#else
		u32 done_age = RADEON_READ( RADEON_LAST_DISPATCH_REG );
#endif
#if ROTATE_BUFS
		for ( i = start ; i < dma->buf_count ; i++ ) {
#else
		for ( i = 0 ; i < dma->buf_count ; i++ ) {
#endif
			buf = dma->buflist[i];
			buf_priv = buf->dev_private;
			if ( buf->pending && buf_priv->age <= done_age ) {
				/* The buffer has been processed, so it
				 * can now be used.
				 */
				buf->pending = 0;
				DRM_DEBUG( "  ret buf=%d last=%d age=%d done=%d\n", buf->idx, dev_priv->last_buf, buf_priv->age, done_age );
				return buf;
			}
			DRM_DEBUG( "    skipping buf=%d age=%d done=%d\n",
				   buf->idx, buf_priv->age,
				   done_age );
#if ROTATE_BUFS
			start = 0;
#endif
		}
		DRM_OS_DELAY( 1 );
	}

	DRM_ERROR( "returning NULL!\n" );
	return NULL;
}

void radeon_freelist_reset( drm_device_t *dev )
{
	drm_device_dma_t *dma = dev->dma;
#if ROTATE_BUFS
	drm_radeon_private_t *dev_priv = dev->dev_private;
#endif
	int i;

#if ROTATE_BUFS
	dev_priv->last_buf = 0;
#endif
	for ( i = 0 ; i < dma->buf_count ; i++ ) {
		drm_buf_t *buf = dma->buflist[i];
		drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
		buf_priv->age = 0;
	}
}


/* ================================================================
 * CP command submission
 */

int radeon_wait_ring( drm_radeon_private_t *dev_priv, int n )
{
	drm_radeon_ring_buffer_t *ring = &dev_priv->ring;
	int i;

	for ( i = 0 ; i < dev_priv->usec_timeout ; i++ ) {
		radeon_update_ring_snapshot( ring );
		if ( ring->space > n )
			return 0;
		DRM_OS_DELAY( 1 );
	}

	/* FIXME: This return value is ignored in the BEGIN_RING macro! */
#if RADEON_FIFO_DEBUG
	radeon_status( dev_priv );
	DRM_ERROR( "failed!\n" );
#endif
	return DRM_OS_ERR(EBUSY);
}

static int radeon_cp_get_buffers( drm_device_t *dev, drm_dma_t *d )
{
	int i;
	drm_buf_t *buf;

	for ( i = d->granted_count ; i < d->request_count ; i++ ) {
		buf = radeon_freelist_get( dev );
		if ( !buf ) return DRM_OS_ERR(EAGAIN);

		buf->pid = DRM_OS_CURRENTPID;

		if (DRM_OS_COPYTOUSR( &d->request_indices[i], &buf->idx,
				   sizeof(buf->idx) ) )
			return DRM_OS_ERR(EFAULT);
		if (DRM_OS_COPYTOUSR( &d->request_sizes[i], &buf->total,
				   sizeof(buf->total) ) )
			return DRM_OS_ERR(EFAULT);

		d->granted_count++;
	}
	return 0;
}

int radeon_cp_buffers( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	int ret = 0;
	drm_dma_t d;

	LOCK_TEST_WITH_RETURN( dev );

	DRM_OS_KRNFROMUSR( d, (drm_dma_t *) data, sizeof(d) );

	/* Please don't send us buffers.
	 */
	if ( d.send_count != 0 ) {
		DRM_ERROR( "Process %d trying to send %d buffers via drmDMA\n",
			   DRM_OS_CURRENTPID, d.send_count );
		return DRM_OS_ERR(EINVAL);
	}

	/* We'll send you buffers.
	 */
	if ( d.request_count < 0 || d.request_count > dma->buf_count ) {
		DRM_ERROR( "Process %d trying to get %d buffers (of %d max)\n",
			   DRM_OS_CURRENTPID, d.request_count, dma->buf_count );
		return DRM_OS_ERR(EINVAL);
	}

	d.granted_count = 0;

	if ( d.request_count ) {
		ret = radeon_cp_get_buffers( dev, &d );
	}

	DRM_OS_KRNTOUSR( (drm_dma_t *) data, d, sizeof(d) );

	return ret;
}
