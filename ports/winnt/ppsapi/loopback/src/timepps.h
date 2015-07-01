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
		while ((x).tv_nsec >= PPS_NANOSECOND) { \
			(x).tv_nsec -= PPS_NANOSECOND; \
			(x).tv_sec++; \
		} \
		while ((x).tv_nsec < 0) { \
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
		d_frac = (double)((ntp_fp_t *)&(x))->F.u		\
			* PPS_NANOSECOND / PPS_FRAC;			\
		(x).tv_sec = ((ntp_fp_t *)&(x))->I.u			\
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

typedef struct ntp_fp {
	union {
		int32   s;
		u_int32 u;
	} I;
	union {
		int32   s;
		u_int32 u;
	} F;
} ntp_fp_t;				/* NTP-compatible time stamp */

#pragma warning(pop)

typedef union pps_timeu {		/* timestamp format */
	struct timespec tspec;
	ntp_fp_t	ntpfp;
	unsigned long	longpad[3];
} pps_timeu_t;				/* generic data type to represent time stamps */

/* addition of NTP fixed-point format */
static void
ntpfp_add(			/* *op1r += *op2 */
	ntp_fp_t       *op1r,
	const ntp_fp_t *op2 )
{
	op1r->F.u += op2->F.u;
	op1r->I.u += op2->I.u + (op1r->F.u < op2->F.u);
}

#define	NTPFP_L_ADDS ntpfp_add


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
#include <stddef.h>	/* offsetof() */
#include <io.h>		/* _get_osfhandle() */

#ifndef EOPNOTSUPP
#define EOPNOTSUPP 45
#endif

typedef UINT_PTR pps_handle_t;	/* pps handlebars */

#ifndef inline
#define inline __inline
#endif

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

static ppsapi_provider *	g_provider_list;
static ppsapi_provider *	g_curr_provider;


static inline pps_handle_t
internal_create_pps_handle(
	void *	prov_context
	)
{
	pps_unit_t *	punit;

	if (NULL == g_curr_provider) {
		fprintf(stderr, "create_pps_handle: provider backend called me outside time_pps_create\n");
		punit = NULL;
	}	else
		punit = malloc(sizeof(*punit));
	if (punit != NULL) {
		punit->provider = g_curr_provider;
		punit->context = prov_context;
		punit->magic = PPSAPI_MAGIC_UNIT;
		memset(&punit->params, 0, sizeof(punit->params));
	}
	return (pps_handle_t)punit;
}

static inline pps_unit_t *
unit_from_ppsapi_handle(
	pps_handle_t	handle
	)
{
	pps_unit_t *punit;

	punit = (pps_unit_t *)handle;
	if (PPSAPI_MAGIC_UNIT != punit->magic)
		punit = NULL;
	return punit;
}

/*
 * ntpd on Windows only looks to errno after finding
 * GetLastError returns NO_ERROR.  To accomodate its
 * use of msyslog in portable code such as refclock_atom.c,
 * this implementation always clears the Windows
 * error code using SetLastError(NO_ERROR) when
 * returning an errno.  This is also a good idea
 * for any non-ntpd clients as they should rely only
 * the errno for PPSAPI functions.
 */
static __inline  int
pps_set_errno(
	int e)
{
	SetLastError(NO_ERROR);
	errno = e;
	return -1;
}

#define	RETURN_PPS_ERRNO(e) return pps_set_errno(e)


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

	(void)Counterstamp;

	/* convert from 100ns units to NTP fixed point format */

	BiasedTimestamp = Timestamp - PPS_FILETIME_1970;
	result->I.u = PPS_JAN_1970 + 
		(unsigned)(BiasedTimestamp / PPS_HECTONANOSECONDS);
	result->F.u = 
		(unsigned) ((BiasedTimestamp % PPS_HECTONANOSECONDS) *
		(PPS_FRAC / PPS_HECTONANOSECONDS));
}
#endif


static inline int
load_pps_provider(
	char *	dllpath
	)
{
	char			short_name[16];
	char			full_name[64];
	ppsapi_provider *	prov;
	HMODULE			hmod;
	pppsapi_prov_init	pprov_init;

	prov = malloc(sizeof(*prov));
	if (NULL == prov)
		return ENOMEM;

	hmod = LoadLibrary(dllpath);
	if (NULL == hmod) {
		fprintf(stderr, "load_pps_provider: LoadLibrary(%s) error %u\n", dllpath, GetLastError());
		free(prov);
		return ENOENT;
	}

	pprov_init = (pppsapi_prov_init)GetProcAddress(hmod, "ppsapi_prov_init");
	if (NULL == pprov_init) {
		fprintf(stderr, "load_pps_provider: entrypoint ppsapi_prov_init not found in %s\n", dllpath);
		free(prov);
		FreeLibrary(hmod);
		return EFAULT;
	}

	prov->caps = (*pprov_init)(PPSAPI_TIMEPPS_PROV_VER,
	    &internal_create_pps_handle,
	    &pps_ntp_timestamp_from_counter,
	    short_name,  sizeof(short_name),
	    full_name, sizeof(full_name));

	if (!prov->caps) {
		free(prov);
		FreeLibrary(hmod);
		return EACCES;
	}

	prov->short_name = _strdup(short_name);
	prov->full_name = _strdup(full_name);

	if (NULL == prov->short_name || !prov->short_name[0]
	    || NULL == prov->full_name || !prov->full_name[0]) {

		if (prov->short_name)
			free(prov->short_name);
		if (prov->full_name)
			free(prov->full_name);
		free(prov);
		FreeLibrary(hmod);
		return EINVAL;
	}

	prov->ptime_pps_create = (provtime_pps_create)
		GetProcAddress(hmod, "prov_time_pps_create");
	prov->ptime_pps_destroy = (provtime_pps_destroy)
		GetProcAddress(hmod, "prov_time_pps_destroy");
	prov->ptime_pps_setparams = (provtime_pps_setparams)
		GetProcAddress(hmod, "prov_time_pps_setparams");
	prov->ptime_pps_fetch = (provtime_pps_fetch)
		GetProcAddress(hmod, "prov_time_pps_fetch");
	prov->ptime_pps_kcbind = (provtime_pps_kcbind)
		GetProcAddress(hmod, "prov_time_pps_kcbind");

	if (NULL == prov->ptime_pps_create
	    || NULL == prov->ptime_pps_destroy
	    || NULL == prov->ptime_pps_setparams
	    || NULL == prov->ptime_pps_fetch
	    || NULL == prov->ptime_pps_kcbind) {

		fprintf(stderr, "PPSAPI provider %s missing entrypoint\n",
			prov->short_name);
		free(prov->short_name);
		free(prov->full_name);
		free(prov);
		FreeLibrary(hmod);
		return EINVAL;
	}

	fprintf(stderr, "loaded PPSAPI provider %s caps 0x%x provider %p\n", 
		prov->full_name, prov->caps, prov);

	prov->next = g_provider_list;
	g_provider_list = prov;

	return 0;
}


/*
 * time_pps_create - create PPS handle from file descriptor
 *
 * This is the initial entrypoint of PPSAPI from the client.  Note
 * to maintain source compatibility with Unix, the input file
 * descriptor really is a descriptor from the C runtime low-numbered
 * descriptor namespace, though it may have been converted from a
 * native Windows HANDLE using _open_osfhandle().
 */
static inline int
time_pps_create(
	int		filedes,/* device file descriptor */
	pps_handle_t *	phandle	/* returned handle */
	)
{
	HANDLE			winhandle;
	char *			dlls;
	char *			dll;
	char *			pch;
	ppsapi_provider *	prov;
	pps_handle_t		ppshandle;
	int			err;

	if (NULL == phandle)
		RETURN_PPS_ERRNO(EFAULT);

	winhandle = (HANDLE)_get_osfhandle(filedes);
	fprintf(stderr, "time_pps_create(%d) got winhandle %p\n", filedes, winhandle);
	if (INVALID_HANDLE_VALUE == winhandle)
		RETURN_PPS_ERRNO(EBADF);

	/*
	 * For initial testing the list of PPSAPI backend
	 * providers is provided by the environment variable
	 * PPSAPI_DLLS, separated by semicolons such as
	 * PPSAPI_DLLS=c:\ntp\serial_ppsapi.dll;..\parport_ppsapi.dll
	 * There are a million better ways, such as a well-known
	 * registry key under which a value is created for each
	 * provider DLL installed, or even a platform-specific
	 * ntp.conf directive or command-line switch.
	 */
	dlls = getenv("PPSAPI_DLLS");
	if (dlls != NULL && NULL == g_provider_list) {
		dlls = dll = _strdup(dlls);
		fprintf(stderr, "getenv(PPSAPI_DLLS) gives %s\n", dlls);
	} else
		dlls = dll = NULL;

	while (dll != NULL && dll[0]) {
		pch = strchr(dll, ';');
		if (pch != NULL)
			*pch = 0;
		err = load_pps_provider(dll);
		if (err) {
			fprintf(stderr, "load_pps_provider(%s) got errno %d\n", dll, err);
			RETURN_PPS_ERRNO(err);
		}
		dll = (NULL == pch)
			  ? NULL
			  : pch + 1;
	}

	if (NULL != dlls)
		free(dlls);
	dlls = dll = NULL;

	/*
	 * Hand off to each provider in turn until one returns a PPS
	 * handle or they've all declined.
	 */
	for (prov = g_provider_list; prov != NULL; prov = prov->next) {
		ppshandle = 0;
		g_curr_provider = prov;
		err = (*prov->ptime_pps_create)(winhandle, &ppshandle);
		g_curr_provider = NULL;
		fprintf(stderr, "%s prov_time_pps_create(%p) returned %d\n",
			prov->short_name, winhandle, err);
		if (!err && ppshandle) {
			*phandle = ppshandle;
			return 0;
		}
	}

	fprintf(stderr, "PPSAPI provider list %p\n", g_provider_list);

	RETURN_PPS_ERRNO(ENOEXEC);
}


/*
 * release PPS handle
 */

static inline int
time_pps_destroy(
	pps_handle_t handle
	)
{
	pps_unit_t *	punit;
	int err;

	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	punit = unit_from_ppsapi_handle(handle);

	if (NULL == punit)
		RETURN_PPS_ERRNO(EBADF);

	err = (*punit->provider->ptime_pps_destroy)(punit, punit->context);

	free(punit);

	if (err)
		RETURN_PPS_ERRNO(err);
	else
		return 0;
}

/*
 * set parameters for handle
 */

static inline int
time_pps_setparams(
	pps_handle_t handle,
	const pps_params_t *params
	)
{
	pps_unit_t *	punit;
	int		err;

	/*
	 * Check for valid arguments and set parameters.
	 */
	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	punit = unit_from_ppsapi_handle(handle);

	if (NULL == punit)
		RETURN_PPS_ERRNO(EBADF);

	if (NULL == params)
		RETURN_PPS_ERRNO(EFAULT);

	err = (*punit->provider->ptime_pps_setparams)(punit, punit->context, params);

	if (err)
		RETURN_PPS_ERRNO(err);
	else
		return 0;
}

/*
 * get parameters for handle
 */

static inline int
time_pps_getparams(
	pps_handle_t handle,
	pps_params_t *params_buf
	)
{
	pps_unit_t *	punit;

	/*
	 * Check for valid arguments and get parameters.
	 */
	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	punit = unit_from_ppsapi_handle(handle);

	if (NULL == punit)
		RETURN_PPS_ERRNO(EBADF);

	if (NULL == params_buf)
		RETURN_PPS_ERRNO(EFAULT);

	*params_buf = punit->params;
	return 0;
}


/* 
 * time_pps_getcap - get capabilities for handle
 */
static inline int
time_pps_getcap(
	pps_handle_t handle,
	int *pmode
	)
{
	pps_unit_t *	punit;

	/*
	 * Check for valid arguments and get capabilities.
	 */
	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	punit = unit_from_ppsapi_handle(handle);

	if (NULL == punit)
		RETURN_PPS_ERRNO(EBADF);

	if (NULL == pmode)
		RETURN_PPS_ERRNO(EFAULT);

	*pmode = punit->provider->caps;
	return 0;
}

/*
 * Fetch timestamps
 */

static inline int
time_pps_fetch(
	pps_handle_t		handle,
	const int		tsformat,
	pps_info_t *		pinfo,
	const struct timespec *	ptimeout
	)
{
	pps_unit_t *	punit;
	int		err;

	/*
	 * Check for valid arguments and fetch timestamps
	 */
	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	if (NULL == pinfo)
		RETURN_PPS_ERRNO(EFAULT);

	punit = unit_from_ppsapi_handle(handle);

	if (NULL == punit)
		RETURN_PPS_ERRNO(EBADF);

	err = (*punit->provider->ptime_pps_fetch)(punit,
						  punit->context, 
						  tsformat, 
						  pinfo, 
						  ptimeout);

	if (err)
		RETURN_PPS_ERRNO(err);
	else
		return 0;
}

/*
 * time_pps_kcbind - specify kernel consumer
 *
 * Not supported so far by Windows.
 */

static inline int
time_pps_kcbind(
	pps_handle_t handle,
	const int kernel_consumer,
	const int edge, const int tsformat
	)
{
	pps_unit_t *	punit;
	int		err;

	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	punit = unit_from_ppsapi_handle(handle);

	if (NULL == punit)
		RETURN_PPS_ERRNO(EBADF);

	err = (*punit->provider->ptime_pps_kcbind)(
		punit,
		punit->context,
		kernel_consumer,
		edge,
		tsformat);

	if (err)
		RETURN_PPS_ERRNO(err);
	else
		return 0;
}


#endif /* TIMEPPS_H */
