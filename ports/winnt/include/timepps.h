/***********************************************************************
 *								       *
 * Copyright (c) David L. Mills 1999-2009			       *
 *								       *
 * Permission to use, copy, modify, and distribute this software and   *
 * its documentation for any purpose and with or without fee is hereby *
 * granted, provided that the above copyright notice appears in all    *
 * copies and that both the copyright notice and this permission       *
 * notice appear in supporting documentation, and that the name        *
 * University of Delaware not be used in advertising or publicity      *
 * pertaining to distribution of the software without specific,        *
 * written prior permission. The University of Delaware makes no       *
 * representations about the suitability this software for any	       *
 * purpose. It is provided "as is" without express or implied          *
 * warranty.							       *
 *								       *
 ***********************************************************************
 *								       *
 * This header file complies with "Pulse-Per-Second API for UNIX-like  *
 * Operating Systems, Version 1.0", rfc2783. Credit is due Jeff Mogul  *
 * and Marc Brett, from whom much of this code was shamelessly stolen. *
 *								       *
 * This modified timepps.h can be used to provide a PPSAPI interface   *
 * to a machine running Windows with one or more backend provider DLLs *
 * implementing the provider interfaces defined herein.		       *
 *								       *
 * This Windows version was derived by Dave Hart		       *
 * <davehart@davehart.com> from Mills' timepps-Solaris.h	       *
 *								       *
 ***********************************************************************
 *								       *
 * Some of this include file					       *
 * Copyright (c) 1999 by Ulrich Windl,				       *
 *	based on code by Reg Clemens <reg@dwf.com>		       *
 *		based on code by Poul-Henning Kamp <phk@FreeBSD.org>   *
 *								       *
 ***********************************************************************
 *								       *
 * "THE BEER-WARE LICENSE" (Revision 42):			       *
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this      *
 * notice you can do whatever you want with this stuff. If we meet some*
 * day, and you think this stuff is worth it, you can buy me a beer    *
 * in return.	Poul-Henning Kamp				       *
 *								       *
 **********************************************************************/

#ifndef TIMEPPS_H
#define TIMEPPS_H

#include "sys/time.h"	/* in ntp ref source declares struct timespec */

/*
 * The following definitions are architecture independent
 */

#define PPS_API_VERS_1		1		/* API version number */
#define PPS_JAN_1970		2208988800UL	/* 1970 - 1900 in seconds */
#define PPS_NANOSECOND		1000000000L	/* one nanosecond in decimal */
#define PPS_FRAC		4294967296.	/* 2^32 as a double */
#define PPS_HECTONANOSECONDS	10000000	/* 100ns units in a second */
#define PPS_FILETIME_1970	0x019db1ded53e8000 /* unix epoch to Windows */

#define PPS_NORMALIZE(x)	/* normalize timespec */ \
	do { \
		if ((x).tv_nsec >= PPS_NANOSECOND) { \
			(x).tv_nsec -= PPS_NANOSECOND; \
			(x).tv_sec++; \
		} else if ((x).tv_nsec < 0) { \
			(x).tv_nsec += PPS_NANOSECOND; \
			(x).tv_sec--; \
		} \
	} while (0)

#define PPS_TSPECTONTP(x)	/* convert timespec to ntp_fp */	\
	do {								\
		double d_frac;						\
									\
		d_frac = ((struct timespec)&(x))->tv_nsec		\
			 * PPS_FRAC / PPS_NANOSECOND;			\
		(x).integral = ((struct timespec)&(x))->tv_sec		\
				+ PPS_JAN_1970;				\
		(x).fractional = (unsigned int)d_frac;			\
		if (d_frac >= PPS_FRAC)					\
			(x).integral++;					\
	} while (0)

#define PPS_NTPTOTSPEC(x)	/* convert ntp_fp to timespec */	\
	do {								\
		double d_frac;						\
									\
		/* careful, doing in place and tv_sec may be 64bit */	\
		d_frac = (double)((ntp_fp_t *)&(x))->fractional		\
			* PPS_NANOSECOND / PPS_FRAC;			\
		(x).tv_sec = ((ntp_fp_t *)&(x))->integral		\
			     - (time_t)PPS_JAN_1970;			\
		(x).tv_nsec = (long)d_frac;				\
	} while (0) 


/*
 * Device/implementation parameters (mode)
 */

#define PPS_CAPTUREASSERT	0x01	/* capture assert events */
#define PPS_CAPTURECLEAR	0x02	/* capture clear events */
#define PPS_CAPTUREBOTH 	0x03	/* capture assert and clear events */

#define PPS_OFFSETASSERT	0x10	/* apply compensation for assert ev. */
#define PPS_OFFSETCLEAR 	0x20	/* apply compensation for clear ev. */
#define PPS_OFFSETBOTH		0x30	/* apply compensation for both */

#define PPS_CANWAIT		0x100	/* Can we wait for an event? */
#define PPS_CANPOLL		0x200	/* "This bit is reserved for */

/*
 * Kernel actions (mode)
 */

#define PPS_ECHOASSERT		0x40	/* feed back assert event to output */
#define PPS_ECHOCLEAR		0x80	/* feed back clear event to output */

/*
 * Timestamp formats (tsformat)
 */

#define PPS_TSFMT_TSPEC		0x1000	/* select timespec format */
#define PPS_TSFMT_NTPFP		0x2000	/* select NTP format */
#define PPS_TSFMT_BOTH		(PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP)

/*
 * Kernel discipline actions (not used in Windows yet)
 */

#define PPS_KC_HARDPPS		0	/* enable kernel consumer */
#define PPS_KC_HARDPPS_PLL	1	/* phase-lock mode */
#define PPS_KC_HARDPPS_FLL	2	/* frequency-lock mode */

/*
 * Type definitions
 */

typedef unsigned long pps_seq_t;	/* sequence number */

#pragma warning(push)
#pragma warning(disable: 201)		/* nonstd extension nameless union */

typedef struct ntp_fp {
	union ntp_fp_sec {
		unsigned int	integral;
		int		s_integral;
	};
	unsigned int	fractional;
} ntp_fp_t;				/* NTP-compatible time stamp */

#pragma warning(pop)

typedef union pps_timeu {		/* timestamp format */
	struct timespec tspec;
	ntp_fp_t	ntpfp;
	unsigned long	longpad[3];
} pps_timeu_t;				/* generic data type to represent time stamps */

/* addition of NTP fixed-point format */

#define NTPFP_M_ADD(r_i, r_f, a_i, a_f) 	/* r += a */ \
	do { \
		register u_int32 lo_tmp; \
		register u_int32 hi_tmp; \
		\
		lo_tmp = ((r_f) & 0xffff) + ((a_f) & 0xffff); \
		hi_tmp = (((r_f) >> 16) & 0xffff) + (((a_f) >> 16) & 0xffff); \
		if (lo_tmp & 0x10000) \
			hi_tmp++; \
		(r_f) = ((hi_tmp & 0xffff) << 16) | (lo_tmp & 0xffff); \
		\
		(r_i) += (a_i); \
		if (hi_tmp & 0x10000) \
			(r_i)++; \
	} while (0)

#define	NTPFP_L_ADDS(r, a)	NTPFP_M_ADD((r)->integral, (r)->fractional, \
					    (a)->s_integral, (a)->fractional)


/*
 * Timestamp information structure
 */

typedef struct pps_info {
	pps_seq_t	assert_sequence;	/* seq. num. of assert event */
	pps_seq_t	clear_sequence; 	/* seq. num. of clear event */
	pps_timeu_t	assert_tu;		/* time of assert event */
	pps_timeu_t	clear_tu;		/* time of clear event */
	int		current_mode;		/* current mode bits */
} pps_info_t;

#define assert_timestamp	assert_tu.tspec
#define clear_timestamp 	clear_tu.tspec

#define assert_timestamp_ntpfp	assert_tu.ntpfp
#define clear_timestamp_ntpfp	clear_tu.ntpfp

/*
 * Parameter structure
 */

typedef struct pps_params {
	int		api_version;	/* API version # */
	int		mode;		/* mode bits */
	pps_timeu_t	assert_off_tu;	/* offset compensation for assert */
	pps_timeu_t	clear_off_tu;	/* offset compensation for clear */
} pps_params_t;

#define assert_offset		assert_off_tu.tspec
#define clear_offset		clear_off_tu.tspec

#define assert_offset_ntpfp	assert_off_tu.ntpfp
#define clear_offset_ntpfp	clear_off_tu.ntpfp

/*
 *------ Here begins the implementation-specific part! ------
 */

#include <windows.h>
#include <errno.h>

#ifndef EOPNOTSUPP
#define EOPNOTSUPP 45
#endif

typedef UINT_PTR pps_handle_t;	/* pps handlebars */

/*
 * ntpd on Windows is typically distributed as a binary as few users
 * have the tools needed to build from source.  Rather than build
 * a single timepps.h for Windows which knows how to talk to all
 * PPS implementations frozen in time as of compiling, this timepps.h
 * allows one or more backend providers to be used by naming a DLL
 * which exports the provider interfaces defined here.
 */
typedef enum ppsapi_magic_tag {
	PPSAPI_MAGIC_UNIT = 0x70707355,	/* ppsU */
} ppsapi_magic;

typedef struct {
	struct pps_provider_tag *provider;
	void *		context;/* provider's unit pointer */
	ppsapi_magic	magic;	/* to detect invalid handles */
	pps_params_t	params;	/* PPS parameters set by user */
} pps_unit_t;

typedef void (*ppps_ntp_timestamp_from_counter)(
	ntp_fp_t	*result, 
	ULONGLONG	Timestamp, 
	ULONGLONG	Counterstamp
	);

typedef pps_handle_t (*pcreate_pps_handle)(
	void *	prov_context
	);

/*
 * ppsapi_prov_init() - exported by backend DLLs
 *
 * Return value is pps capabilities available to PPSAPI consumers
 * via time_pps_getcaps().
 */
#define PPSAPI_TIMEPPS_PROV_VER		2

typedef int (WINAPI *pppsapi_prov_init)(
	int	ppsapi_timepps_prov_ver,
	pcreate_pps_handle	create_pps_handle,
	ppps_ntp_timestamp_from_counter ntp_timestamp_from_counter,
	char *	short_name_buf,
	size_t	short_name_size,
	char *	full_name_buf,
	size_t	full_name_size
	);

typedef int (WINAPI *provtime_pps_create)(
	HANDLE winhandle,	/* user device handle */
	pps_handle_t *phandle	/* returned handle */
	);

typedef int (WINAPI *provtime_pps_destroy)(
	pps_unit_t *	unit,
	void *		context
	);

typedef int (WINAPI *provtime_pps_setparams)(
	pps_unit_t *		unit,
	void *			context,
	const pps_params_t *	params
	);

typedef int (WINAPI *provtime_pps_fetch)(
	pps_unit_t *		unit,
	void *			context,
	const int		tsformat,
	pps_info_t *		pinfo,
	const struct timespec *	timeout
	);

typedef int (WINAPI *provtime_pps_kcbind)(
	pps_unit_t *	unit,
	void *		context,
	const int	kernel_consumer,
	const int	edge, 
	const int	tsformat
	);

typedef struct pps_provider_tag {
	struct pps_provider_tag *next;
	int			caps;
	char *			short_name;
	char *			full_name;
	provtime_pps_create	ptime_pps_create;
	provtime_pps_destroy	ptime_pps_destroy;
	provtime_pps_setparams	ptime_pps_setparams;
	provtime_pps_fetch	ptime_pps_fetch;
	provtime_pps_kcbind	ptime_pps_kcbind;
} ppsapi_provider;


#ifdef OWN_PPS_NTP_TIMESTAMP_FROM_COUNTER
extern void pps_ntp_timestamp_from_counter(ntp_fp_t *, ULONGLONG, ULONGLONG);
#else
/*
 * helper routine for serialpps.sys ioctl which returns 
 * performance counter "timestamp" as well as a system
 * FILETIME timestamp.  Converts one of the inputs to
 * NTP fixed-point format.
 *
 * You will probably want to supply your own and #define
 * OWN_PPS_NTP_TIMESTAMP_FROM_COUNTER, as this stub
 * converts only the low-resolution system timestamp.
 *
 * When implementing a provider, use the pointer to this
 * conversion function supplied to your prov_init(), as
 * the copy in your DLL will likely be the stub below,
 * where you want the one provided by the PPSAPI client
 * such as ntpd.
 */
static inline void 
pps_ntp_timestamp_from_counter(
	ntp_fp_t	*result, 
	ULONGLONG	Timestamp, 
	ULONGLONG	Counterstamp)
{
	ULONGLONG BiasedTimestamp;

	/* convert from 100ns units to NTP fixed point format */

	BiasedTimestamp = Timestamp - PPS_FILETIME_1970;
	result->integral = PPS_JAN_1970 + 
		(unsigned)(BiasedTimestamp / PPS_HECTONANOSECONDS);
	result->fractional = 
		(unsigned) ((BiasedTimestamp % PPS_HECTONANOSECONDS) *
		(PPS_FRAC / PPS_HECTONANOSECONDS));
}
#endif



/*
 * time_pps_create - create PPS handle from file descriptor
 *
 * This is the initial entrypoint of PPSAPI from the client.  Note
 * to maintain source compatibility with Unix, the input file
 * descriptor really is a descriptor from the C runtime low-numbered
 * descriptor namespace, though it may have been converted from a
 * native Windows HANDLE using _open_osfhandle().
 */
extern int
time_pps_create(
	int		filedes,/* device file descriptor */
	pps_handle_t *	phandle	/* returned handle */
	);

/*
 * release PPS handle
 */
extern int
time_pps_destroy(
	pps_handle_t handle
	);

/*
 * set parameters for handle
 */
extern int
time_pps_setparams(
	pps_handle_t handle,
	const pps_params_t *params
	);

/*
 * get parameters for handle
 */
extern int
time_pps_getparams(
	pps_handle_t handle,
	pps_params_t *params_buf
	);

/* 
 * time_pps_getcap - get capabilities for handle
 */
extern int
time_pps_getcap(
	pps_handle_t handle,
	int *pmode
	);

/*
 * Fetch timestamps
 */
extern int
time_pps_fetch(
	pps_handle_t		handle,
	const int		tsformat,
	pps_info_t *		pinfo,
	const struct timespec *	ptimeout
	);

/*
 * time_pps_kcbind - specify kernel consumer
 *
 * Not supported so far by Windows.
 */
extern int
time_pps_kcbind(
	pps_handle_t handle,
	const int kernel_consumer,
	const int edge, const int tsformat
	);


#endif /* TIMEPPS_H */
