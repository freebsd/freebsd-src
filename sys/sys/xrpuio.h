/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/sys/xrpuio.h,v 1.2.26.1 2005/01/31 23:26:57 imp Exp $
 *
 */

#ifndef _SYS_XRPUIO_H_
#define _SYS_XRPUIO_H_

#include <sys/ioccom.h>

#define XRPU_MAX_PPS	16
struct xrpu_timecounting {

	/* The timecounter itself */
	u_int		xt_addr_trigger;
	u_int		xt_addr_latch;
	unsigned	xt_mask;
	u_int32_t	xt_frequency;
	char		xt_name[16];

	/* The PPS latches */
	struct {
		u_int	xt_addr_assert;
		u_int	xt_addr_clear;
	} xt_pps[XRPU_MAX_PPS];
};

#define XRPU_IOC_TIMECOUNTING _IOW('6', 1, struct xrpu_timecounting)

#endif /* _SYS_XRPUIO_H_ */
