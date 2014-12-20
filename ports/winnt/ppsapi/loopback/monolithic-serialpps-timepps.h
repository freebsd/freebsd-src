/***********************************************************************
 *								       *
 * Copyright (c) David L. Mills 1999-2000			       *
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
 * to a machine running Windows with a suitably modified 	       *
 * serialpps.sys being used in place of serial.sys.  It can	       *
 * be extended to support a modified parallel port driver once	       *
 * available.							       *
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
 * "THE BEER-WARE LICENSE" (Revision 42):                              *
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this      *
 * notice you can do whatever you want with this stuff. If we meet some*
 * day, and you think this stuff is worth it, you can buy me a beer    *
 * in return.	Poul-Henning Kamp				       *
 *								       *
 **********************************************************************/

#ifndef _SYS_TIMEPPS_H_
#define _SYS_TIMEPPS_H_

/* Implementation note: the logical states ``assert'' and ``clear''
 * are implemented in terms of the UART register, i.e. ``assert''
 * means the bit is set.
 */

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

#define PPS_TSPECTONTP(x)	/* convert timespec to ntp_fp */ \
	do { \
		double d_temp; \
	\
		(x).integral += (unsigned int)PPS_JAN_1970; \
		d_temp = (x).fractional * PPS_FRAC / PPS_NANOSECOND; \
		if (d_temp >= PPS_FRAC) \
			(x).integral++; \
		(x).fractional = (unsigned int)d_temp; \
	} while (0)

#define PPS_NTPTOTSPEC(x)	/* convert ntp_fp to timespec */ \
	do { \
		double d_temp; \
	\
		(x).tv_sec -= (time_t)PPS_JAN_1970; \
		d_temp = (double)((x).tv_nsec); \
		d_temp *= PPS_NANOSECOND; \
		d_temp /= PPS_FRAC; \
		(x).tv_nsec = (long)d_temp; \
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

#define PPS_TSFMT_TSPEC 	0x1000	/* select timespec format */
#define PPS_TSFMT_NTPFP 	0x2000	/* select NTP format */

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
 * The following definitions are architecture-dependent
 */

#define PPS_CAP (PPS_CAPTUREASSERT | PPS_OFFSETASSERT | PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP)
#define PPS_RO	(PPS_CANWAIT | PPS_CANPOLL)

typedef struct {
	int filedes;		/* file descriptor */
	OVERLAPPED ol;		/* caches ol.hEvent for DeviceIoControl */
	pps_params_t params;	/* PPS parameters set by user */
} pps_unit_t;

typedef pps_unit_t* pps_handle_t; /* pps handlebars */

/*
 *------ Here begins the implementation-specific part! ------
 */

#include <windows.h>
#include <errno.h>

#ifndef EOPNOTSUPP
#define EOPNOTSUPP 45
#endif

typedef struct _OLD_SERIAL_PPS_STAMPS {
   LARGE_INTEGER Timestamp;
   LARGE_INTEGER Counterstamp;
} OLD_SERIAL_PPS_STAMPS, *POLDSERIAL_PPS_STAMPS;

typedef struct _SERIAL_PPS_STAMPS {
   LARGE_INTEGER Timestamp;
   LARGE_INTEGER Counterstamp;
   DWORD SeqNum;
} SERIAL_PPS_STAMPS, *PSERIAL_PPS_STAMPS;

#define IOCTL_SERIAL_GET_PPS_STAMPS	CTL_CODE(FILE_DEVICE_SERIAL_PORT,114,METHOD_BUFFERED,FILE_ANY_ACCESS)

/* byte offset of member m in struct typedef s */
#define PPS_OFFSETOF(m,s)	(size_t)(&((s *)0)->m)

/*
 * ntpd on Windows only looks to errno after finding
 * GetLastError returns NO_ERROR.  To accomodate its
 * use of msyslog in portable code such as refclock_atom.c,
 * this implementation always clears the Windows
 * error code using SetLastError(NO_ERROR) when
 * returning an errno.  This is also a good idea
 * for any non-ntpd clients as they should use only
 * the errno for PPSAPI functions.
 */
#define	RETURN_PPS_ERRNO(e)	\
do {	\
	SetLastError(NO_ERROR);	\
	errno = (e);	\
	return -1;	\
} while (0)


#ifdef OWN_PPS_NTP_TIMESTAMP_FROM_COUNTER
extern void pps_ntp_timestamp_from_counter(ntp_fp_t *, ULONGLONG, ULONGLONG);
#else
/*
 * helper routine for serialpps.sys ioctl which returns 
 * performance counter "timestamp" as well as a system
 * FILETIME timestamp.  Converts one of the inputs to
 * NTP fixed-point format.
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
 * create PPS handle from file descriptor
 */

static inline int
time_pps_create(
	int filedes,		/* file descriptor */
	pps_handle_t *handle	/* returned handle */
	)
{
	OLD_SERIAL_PPS_STAMPS old_pps_stamps;
	DWORD bytes;
	OVERLAPPED ol;

	/*
	 * Check for valid arguments and attach PPS signal.
	 */

	if (!handle)
		RETURN_PPS_ERRNO(EFAULT);

	if (PPS_OFFSETOF(tspec.tv_nsec, pps_timeu_t) != 
	    PPS_OFFSETOF(ntpfp.fractional, pps_timeu_t)) {
		fprintf(stderr,
			"timepps.h needs work, union of \n"
			"unsigned int ntp_fp.integral and\n"
			"time_t timespec.tv_sec accessed\n"
			"interchangeably.\n");
		RETURN_PPS_ERRNO(EFAULT);
	}

	/*
	 * For this ioctl which will never block, we don't want to go
	 * through the overhead of a completion port, so we use an
	 * event handle in the overlapped structure with its 1 bit set.
	 *
	 * From GetQueuedCompletionStatus docs:
	 * Even if you have passed the function a file handle associated 
	 * with a completion port and a valid OVERLAPPED structure, an 
	 * application can prevent completion port notification. This is 
	 * done by specifying a valid event handle for the hEvent member 
	 * of the OVERLAPPED structure, and setting its low-order bit. A 
	 * valid event handle whose low-order bit is set keeps I/O 
	 * completion from being queued to the completion port.
	 */

	ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	ol.hEvent = (HANDLE) ((ULONG_PTR)ol.hEvent | 1);

	if (FALSE == DeviceIoControl(
		(HANDLE)_get_osfhandle(filedes), 
		IOCTL_SERIAL_GET_PPS_STAMPS, 
		NULL, 
		0, 
		&old_pps_stamps, 
		sizeof(old_pps_stamps), 
		&bytes, 
		&ol)
		|| sizeof(old_pps_stamps) != bytes) {

		/* 
		 * If you want to write some dead code this could detect the 
		 * IOCTL being pended, but the driver always has the info
		 * instantly, so ERROR_IO_PENDING isn't a concern.
		 */

		CloseHandle(ol.hEvent);
		fprintf(stderr,
			"time_pps_create: IOCTL_SERIAL_GET_PPS_STAMPS: %d %d\n",
			bytes,
			GetLastError());
		RETURN_PPS_ERRNO(ENXIO);
	}

	/*
	 * Allocate and initialize default unit structure.
	 */

	*handle = malloc(sizeof(pps_unit_t));
	if (!(*handle))
		RETURN_PPS_ERRNO(ENOMEM);

	memset(*handle, 0, sizeof(pps_unit_t));
	(*handle)->filedes = filedes;
	(*handle)->ol.hEvent = ol.hEvent;
	(*handle)->params.api_version = PPS_API_VERS_1;
	(*handle)->params.mode = PPS_CAPTUREASSERT | PPS_TSFMT_TSPEC;
	return (0);
}

/*
 * release PPS handle
 */

static inline int
time_pps_destroy(
	pps_handle_t handle
	)
{
	/*
	 * Check for valid arguments and detach PPS signal.
	 */

	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	CloseHandle(handle->ol.hEvent);
	free(handle);
	return (0);
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
	int	mode, mode_in;
	/*
	 * Check for valid arguments and set parameters.
	 */

	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	if (!params)
		RETURN_PPS_ERRNO(EFAULT);

	/*
	 * There was no reasonable consensu in the API working group.
	 * I require `api_version' to be set!
	 */

	if (params->api_version != PPS_API_VERS_1)
		RETURN_PPS_ERRNO(EINVAL);

	/*
	 * only settable modes are PPS_CAPTUREASSERT and PPS_OFFSETASSERT
	 */

	mode_in = params->mode;

	/*
	 * Only one of the time formats may be selected
	 * if a nonzero assert offset is supplied.
	 */
	if ((mode_in & (PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP)) ==
	    (PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP)) {

		if (handle->params.assert_offset.tv_sec ||
		    handle->params.assert_offset.tv_nsec) 

			RETURN_PPS_ERRNO(EINVAL);

		/*
		 * If no offset was specified but both time
		 * format flags are used consider it harmless
		 * but turn off PPS_TSFMT_NTPFP so getparams
		 * will not show both formats lit.
		 */
		mode_in &= ~PPS_TSFMT_NTPFP;
	}

	/* turn off read-only bits */

	mode_in &= ~PPS_RO;

	/*
	 * test remaining bits, should only have captureassert, 
	 * offsetassert, and/or timestamp format bits.
	 */

	if (mode_in & ~(PPS_CAPTUREASSERT | PPS_OFFSETASSERT |
			PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP))
		RETURN_PPS_ERRNO(EOPNOTSUPP);

	/*
	 * ok, ready to go.
	 */

	mode = handle->params.mode;
	handle->params = *params;
	handle->params.mode = mode | mode_in;
	handle->params.api_version = PPS_API_VERS_1;
	return (0);
}

/*
 * get parameters for handle
 */

static inline int
time_pps_getparams(
	pps_handle_t handle,
	pps_params_t *params
	)
{
	/*
	 * Check for valid arguments and get parameters.
	 */

	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	if (!params)
		RETURN_PPS_ERRNO(EFAULT);

	*params = handle->params;
	return (0);
}

/* (
 * get capabilities for handle
 */

static inline int
time_pps_getcap(
	pps_handle_t handle,
	int *mode
	)
{
	/*
	 * Check for valid arguments and get capabilities.
	 */

	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	if (!mode)
		RETURN_PPS_ERRNO(EFAULT);

	*mode = PPS_CAP;
	return (0);
}

/*
 * Fetch timestamps
 */

static inline int
time_pps_fetch(
	pps_handle_t handle,
	const int tsformat,
	pps_info_t *ppsinfo,
	const struct timespec *timeout
	)
{
	SERIAL_PPS_STAMPS pps_stamps;
	pps_info_t infobuf;
	BOOL rc;
	DWORD bytes;
	DWORD lasterr;

	/*
	 * Check for valid arguments and fetch timestamps
	 */

	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	if (!ppsinfo)
		RETURN_PPS_ERRNO(EFAULT);

	/*
	 * nb. PPS_CANWAIT is NOT set by the implementation, we can totally
	 * ignore the timeout variable.
	 */

	memset(&infobuf, 0, sizeof(infobuf));

	/*
	 * if not captureassert, nothing to return.
	 */

	if (!handle->params.mode & PPS_CAPTUREASSERT) {
		*ppsinfo = infobuf;
		return (0);
	}

	/*
	 * First rev of serialpps.sys didn't support the SeqNum field,
	 * support it by simply returning constant 0 for serial in that case.
	 */
	pps_stamps.SeqNum = 0;

	/*
	 * interrogate (hopefully) serialpps.sys
	 * if it's the standard serial.sys or another driver,
	 * IOCTL_SERIAL_GET_PPS_STAMPS is most likely unknown
	 * and will result in ERROR_INVALID_PARAMETER.
	 */
	bytes = 0;

	rc = DeviceIoControl(
		(HANDLE)_get_osfhandle(handle->filedes), 
		IOCTL_SERIAL_GET_PPS_STAMPS, 
		NULL, 
		0, 
		&pps_stamps, 
		sizeof(pps_stamps), 
		&bytes, 
		&handle->ol);

	if (!rc) {

		lasterr = GetLastError();
		if (ERROR_INVALID_PARAMETER != lasterr) 
			fprintf(stderr, "time_pps_fetch: ioctl err %d\n", 
					lasterr);
		RETURN_PPS_ERRNO(EOPNOTSUPP);

	} else if (bytes != sizeof(pps_stamps) &&
		   bytes != sizeof(OLD_SERIAL_PPS_STAMPS)) {

		fprintf(stderr, 
			"time_pps_fetch: wanted %d or %d bytes got %d from "
			"IOCTL_SERIAL_GET_PPS_STAMPS 0x%x\n" ,
			sizeof(OLD_SERIAL_PPS_STAMPS),
			sizeof(SERIAL_PPS_STAMPS),
			bytes,
			IOCTL_SERIAL_GET_PPS_STAMPS);
		RETURN_PPS_ERRNO(ENXIO);
	}

	/*
	 * pps_ntp_timestamp_from_counter takes the two flavors
	 * of timestamp we have (counter and system time) and
	 * uses whichever it can to give the best NTP fixed-point
	 * conversion.  In ntpd the Counterstamp is typically
	 * used.  A stub implementation in this file simply
	 * converts from Windows Timestamp to NTP fixed-point.
	 */
	pps_ntp_timestamp_from_counter(
		&infobuf.assert_timestamp_ntpfp, 
		pps_stamps.Timestamp.QuadPart, 
		pps_stamps.Counterstamp.QuadPart);

	/*
	 * Note that only assert timestamps
	 * are captured by this interface.
	 */

	infobuf.assert_sequence = pps_stamps.SeqNum;

	/*
	 * Apply offset and translate to specified format
	 */

	switch (tsformat) {
	case PPS_TSFMT_NTPFP:	/* NTP format requires no translation */
		if (handle->params.mode & PPS_OFFSETASSERT) {
			NTPFP_L_ADDS(&infobuf.assert_timestamp_ntpfp, 
				     &handle->params.assert_offset_ntpfp);
		}
		break;		

	case PPS_TSFMT_TSPEC:	/* timespec format requires conversion to nsecs form */
		PPS_NTPTOTSPEC(infobuf.assert_timestamp);
		if (handle->params.mode & PPS_OFFSETASSERT) {
			infobuf.assert_timestamp.tv_sec  += 
				handle->params.assert_offset.tv_sec;
			infobuf.assert_timestamp.tv_nsec += 
				handle->params.assert_offset.tv_nsec;
			PPS_NORMALIZE(infobuf.assert_timestamp);
		}
		break;

	default:
		RETURN_PPS_ERRNO(EINVAL);
	}

	infobuf.current_mode = handle->params.mode;
	*ppsinfo = infobuf;
	return (0);
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
	/*
	 * Check for valid arguments before revealing the ugly truth
	 */
	if (!handle)
		RETURN_PPS_ERRNO(EBADF);

	RETURN_PPS_ERRNO(EOPNOTSUPP);
}



#endif /* _SYS_TIMEPPS_H_ */
