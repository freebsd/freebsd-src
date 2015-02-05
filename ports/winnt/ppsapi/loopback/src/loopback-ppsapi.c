/*
 * loopback-ppsapi-provider.c - derived from monolithic timepps.h
 *				 for usermode PPS by Juergen Perlinger
 */

/***********************************************************************
 *								       *
 * Copyright (c) David L. Mills 1999-2009			       *
 *								       *
 * Permission to use, copy, modify, and distribute this software and   *
 * its documentation for any purpose and without fee is hereby	       *
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
 * This serialpps-ppsapi-provider.dll implements the PPSAPI provider   *
 * for serialpps.sys, which is a very lightly patched Windows	       *
 * serial.sys with CD timestamping support.
 *								       *
 * This Windows version was derived by Dave Hart		       *
 * <davehart@davehart.com> from David L. Mills' timepps-Solaris.h      *
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

/*
 * Implementation note: the logical states ``assert'' and ``clear''
 * are implemented in terms of the UART register, i.e. ``assert''
 * means the bit is set. This follows the sense of the serial driver
 * of the Windows OS, and is opposite of the RS-232 spec for the
 * CD/DCD logical state.
 */


#define PPSAPI_PROVIDER_EXPORTS
#include "loopback-ppsapi.h"

/*
** global stuff
*/

pcreate_pps_handle p_create_pps_handle;

#define SERIALPPS_CAPS	(PPS_CAPTUREBOTH | PPS_OFFSETBOTH | PPS_TSFMT_BOTH)
#define SERIALPPS_RO	(PPS_CANWAIT | PPS_CANPOLL)


/*
 * The ntp_timestamp_from_counter callback into timepps.h routines in
 * the host is saved in each unit separately, so that binaries that
 * inline timepps.h into multiple source files (such as refclock_atom.c
 * and a number of other ntpd refclocks including refclock_nmea.c) will
 * get called back in the correct instance for each unit.  This assumes 
 * that ppsapi_prov_init for subsequent instances happens only after the
 * first instance has completed all time_pps_create() calls it will
 * invoke, which is a safe assumption at least for ntpd.
 */
struct loopback_unit {
	HANDLE		hnd_dev;	/* true device handle	*/
	HANDLE		hnd_pps;	/* loopback handle	*/
	ntp_fp_t	ofs_assert;	/* correction for assert*/
	ntp_fp_t	ofs_clear;	/* correction for clear */
};
typedef struct loopback_unit loopback_unit;

/*
 * --------------------------------------------------------------------
 * DllMain - DLL entrypoint, no-op.
 * --------------------------------------------------------------------
 */
BOOL APIENTRY DllMain(
	HMODULE	hModule,
	DWORD	why,
	LPVOID	lpReserved
	)
{
	UNUSED(hModule);
	UNUSED(lpReserved);
	UNUSED(why);

	return TRUE;
}

/*
 * --------------------------------------------------------------------
 * time conversion and other helpers
 * --------------------------------------------------------------------
 */

/* --------------------------------------------------------------------
 * convert fixed point time stamp to struct timespec, with proper
 * Epoch/Era unfolding around the current time.
 */
static struct timespec
fp_stamp_to_tspec(
	ntp_fp_t	x,
	time_t		p
	)
{
	struct timespec	out;
	u_int64		tmp;
	u_int32		ntp;

	ntp  = x.I.u;
	tmp  = p;
	tmp -= 0x80000000u;		/* unshift of half range */
	ntp -= (u_int32)PPS_JAN_1970;	/* warp into UN*X domain */
	ntp -= (u_int32)tmp;		/* cycle difference	 */
	tmp += (u_int64)ntp;		/* get expanded time	 */
	out.tv_sec  = (time_t)tmp;
	out.tv_nsec = ((long)(((u_int64)x.F.u * PPS_NANOSECOND + 0x80000000u) >> 32));
	if (out.tv_nsec >= PPS_NANOSECOND) {
		out.tv_nsec -= PPS_NANOSECOND;
		out.tv_sec++;
	}
	
	return out;
}

/* --------------------------------------------------------------------
 * convert a duration in struct timespec format to
 * fixed point representation.
 */
static ntp_fp_t
tspec_to_fp(
	    const struct timespec *	ts
	    )
{
	ntp_fp_t	out;
	long		tmp;

	out.I.u = (u_int32)ts->tv_sec;
	tmp     = ts->tv_nsec;
	if (tmp < 0)
		do {
			tmp += PPS_NANOSECOND;
			out.I.u--;
		} while (tmp < 0);
	else if (tmp >= PPS_NANOSECOND)
		do {
			tmp -= PPS_NANOSECOND;
			out.I.u++;
		} while (tmp >= PPS_NANOSECOND);
	out.F.u = (u_int32)((u_int64)tmp << 32) + (PPS_NANOSECOND / 2) / PPS_NANOSECOND;
	return out;
}

/* --------------------------------------------------------------------
 * count number of '1' bits in a u_long
 */
static size_t
popcount(
	u_long	val
	)
{
	size_t	res;

	for (res = 0; val; res++)
		val &= (val - 1);
	return res;
}

/*
 * --------------------------------------------------------------------
 * API function implementation
 * --------------------------------------------------------------------
 */

/* --------------------------------------------------------------------
 * prov_time_pps_create - create PPS handle given underlying device
 */
int WINAPI
prov_time_pps_create(
	HANDLE		device,	/* underlying device */
	pps_handle_t *	handle	/* returned handle */
	)
{
	loopback_unit * loopunit;
	pps_unit_t *	punit;

	/*
	 * Allocate and initialize loopback unit structure.
	 */
	loopunit = (loopback_unit*)calloc(1, sizeof(loopback_unit));
	if (NULL == loopunit)
		return ENOMEM;

	/* Try to attach to NTPD internal data with the device handle.
	 * Free unit buffer on failure.
	 */
	loopunit->hnd_dev = device;
	loopunit->hnd_pps = ntp_pps_attach_device(device);
	if (NULL == loopunit->hnd_pps) {
		free(loopunit);
		return ENXIO;
	}

	/* create the outer PPS handle structure. Undo work done so far
	 * if things go wrong.
	 */
	*handle = (*p_create_pps_handle)(loopunit);
	if (!*handle) {
		ntp_pps_detach_device(loopunit->hnd_pps);
		free(loopunit);
		return ENOMEM;
	}

	/* All good so far. Store things to remember. */
	punit = (pps_unit_t *)*handle;
	punit->params.api_version = PPS_API_VERS_1;
	punit->params.mode = PPS_CAPTUREBOTH | PPS_TSFMT_BOTH;
	return 0;
}


/* --------------------------------------------------------------------
 * prov_time_pps_destroy - release PPS handle
 */
int WINAPI
prov_time_pps_destroy(
	pps_unit_t *	unit,
	void *		context
	)
{
	loopback_unit * loopunit;

	loopunit = (loopback_unit*)context;
	if (unit->context == context)
		unit->context = NULL;
	if (NULL != loopunit)
		ntp_pps_detach_device(loopunit->hnd_pps);
	free(loopunit);
	return 0;
}


/* --------------------------------------------------------------------
 * prov_time_pps_setparams - set parameters for handle
 */
int WINAPI
prov_time_pps_setparams(
	pps_unit_t *		unit,
	void *			context,
	const pps_params_t *	params
	)
{
	loopback_unit *	loopunit;
	int		mode;

	loopunit = (loopback_unit*)context;
	mode     = params->mode;

	/*
	 * There was no reasonable consensus in the API working group.
	 * I require `api_version' to be set!
	 */
	if (params->api_version != PPS_API_VERS_1)
		return EINVAL;

	/*
	 * We support all edges and formats plus offsets, but not
	 * POLL or WAIT. And we are strict on the time stamp format:
	 * Only one is permitted if we set offsets!
	 */

	/*
	 * Only one of the time formats may be selected.
	 */
	if ((mode & PPS_OFFSETBOTH)         != 0 &&
	    popcount(mode & PPS_TSFMT_BOTH) != 1  )
		return EINVAL;

	/* turn off read-only bits */
	mode &= ~SERIALPPS_RO;

	/*
	 * test remaining bits.
	 */
	if (mode & ~(PPS_CAPTUREBOTH | PPS_OFFSETBOTH | PPS_TSFMT_BOTH))
		return EOPNOTSUPP;

	/*
	 * ok, ready to go.
	 *
	 * calculate offsets as ntp_fp_t's and store them in unit as ntp_fp_t. They will
	 * be always applied, since fetching the time stamps is not critical.
	 */
	if (mode & PPS_OFFSETASSERT) {
		if (mode & PPS_TSFMT_TSPEC)
			loopunit->ofs_assert = tspec_to_fp(&params->assert_offset);
		else
			loopunit->ofs_assert = params->assert_offset_ntpfp;
	}
	if (mode & PPS_OFFSETCLEAR) {
		if (mode & PPS_TSFMT_TSPEC)
			loopunit->ofs_clear = tspec_to_fp(&params->clear_offset);
		else
			loopunit->ofs_clear = params->clear_offset_ntpfp;
	}
	/* save remaining bits */
	mode |= unit->params.mode;
	unit->params = *params;
	unit->params.mode = mode;

	return 0;
}


/* --------------------------------------------------------------------
 * prov_time_pps_fetch - Fetch timestamps
 */

int WINAPI
prov_time_pps_fetch(
	pps_unit_t *		unit,
	void *			context,
	const int		tsformat,
	pps_info_t *		pinfo,
	const struct timespec *	timeout
	)
{
	loopback_unit *		loopunit;
	PPSData_t		pps_stamps;
	pps_info_t		infobuf;
	BOOL			rc;
	time_t			tnow;

	/*
	 * nb. PPS_CANWAIT is NOT set by the implementation, we can totally
	 * ignore the timeout variable.
	 */
	UNUSED(timeout);
	loopunit = (loopback_unit*)context;

	/* read & check raw data from daemon */
	memset(&infobuf, 0, sizeof(infobuf));
	rc = ntp_pps_read(loopunit->hnd_pps, &pps_stamps, sizeof(pps_stamps));
	if (!rc) {
		switch (GetLastError())
		{
		case ERROR_INVALID_HANDLE:
			return EINVAL;
		case ERROR_INVALID_PARAMETER:
			return EOPNOTSUPP;
		case ERROR_INVALID_DATA:
			return ENXIO;
		default:
			return EINVAL;
		}
	}

	/* add offsets on raw data */
	NTPFP_L_ADDS(&pps_stamps.ts_assert, &loopunit->ofs_assert);
	NTPFP_L_ADDS(&pps_stamps.ts_clear , &loopunit->ofs_clear);

	/* store sequence numbers */
	infobuf.assert_sequence = pps_stamps.cc_assert;
	infobuf.clear_sequence  = pps_stamps.cc_clear;

	/*
	 * Translate or copy to specified format
	 */
	switch (tsformat) {
	case PPS_TSFMT_NTPFP:	/* NTP format requires no translation */
		infobuf.assert_timestamp_ntpfp = pps_stamps.ts_assert;
		infobuf.clear_timestamp_ntpfp  = pps_stamps.ts_clear;
		break;		

	case PPS_TSFMT_TSPEC:	/* timespec format requires conversion to nsecs form */
		time(&tnow);
		infobuf.assert_timestamp = fp_stamp_to_tspec(pps_stamps.ts_assert, tnow);
		infobuf.clear_timestamp  = fp_stamp_to_tspec(pps_stamps.ts_clear, tnow);
		break;

	default:
		return EINVAL;
	}

	infobuf.current_mode = unit->params.mode;
	*pinfo = infobuf;

	return 0;
}


/* --------------------------------------------------------------------
 * prov_time_pps_kcbind - specify kernel consumer
 *
 * Not supported so far by Windows.
 */
int WINAPI
prov_time_pps_kcbind(
	pps_unit_t *	punit,
	void *		context,
	const int	kernel_consumer,
	const int	edge,
	const int	tsformat
	)
{
	UNUSED(punit);
	UNUSED(context);
	UNUSED(kernel_consumer);
	UNUSED(edge);
	UNUSED(tsformat);

	return EOPNOTSUPP;
}


/* --------------------------------------------------------------------
 * prov_init - returns capabilities and provider name
 */
int WINAPI
ppsapi_prov_init(
	int				ppsapi_timepps_prov_ver,
	pcreate_pps_handle		create_pps_handle,
	ppps_ntp_timestamp_from_counter ntp_timestamp_from_counter,
	char *				short_name_buf,
	size_t				short_name_size,
	char *				full_name_buf,
	size_t				full_name_size
	)
{
	UNUSED(ntp_timestamp_from_counter);

	if (ppsapi_timepps_prov_ver < PPSAPI_TIMEPPS_PROV_VER)
		return 0;

	p_create_pps_handle = create_pps_handle;

	strncpy(short_name_buf, "loopback", short_name_size);
	short_name_buf[short_name_size - 1] ='\0'; /* ensure ASCIIZ */
	strncpy(full_name_buf, 
		"loopback user mode DCD change detection",
		full_name_size);
	full_name_buf[full_name_size - 1] ='\0'; /* ensure ASCIIZ */

	return SERIALPPS_CAPS;
}
