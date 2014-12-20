/*
 * skeleton-ppsapi-provider.c - structure but no useful function
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
 * This skeleton-ppsapi-provider.c implements the PPSAPI provider DLL  *
 * interface but has no actual timestamp-fetching code.  It is	       *
 * derived from serialpps-ppsapi-provider.c which was derived from     *
 * David L. Mills' timepps.h for Solaris.			       *
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


#include "skeleton-ppsapi-provider.h"

pcreate_pps_handle		p_create_pps_handle;
ppps_ntp_timestamp_from_counter	p_ntp_timestamp_from_counter;

#define SKELPPS_CAPS	(PPS_CAPTUREASSERT | PPS_OFFSETASSERT	\
			 | PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP)
#define SKELPPS_RO	(PPS_CANWAIT | PPS_CANPOLL)


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
typedef struct skel_unit_tag {
	HANDLE				device;
	ppps_ntp_timestamp_from_counter	p_ntp_timestamp_from_counter;
} skel_unit;


/*
 * DllMain - DLL entrypoint, no-op.
 */
BOOL APIENTRY DllMain(
	HMODULE	hModule,
	DWORD	ul_reason_for_call,
	LPVOID	lpReserved
	)
{
	UNUSED(hModule);
	UNUSED(ul_reason_for_call);
	UNUSED(lpReserved);

	return TRUE;
}


/*
 * prov_time_pps_create - create PPS handle given underlying device
 */
int WINAPI
prov_time_pps_create(
	HANDLE		device,	/* underlying device */
	pps_handle_t *	handle	/* returned handle */
	)
{
	skel_unit *	pskelunit;
	pps_unit_t *	punit;

	/*
	 * Allocate and initialize unit structure.
	 */

	pskelunit = malloc(sizeof(*pskelunit));
	if (NULL == pskelunit)
		return ENOMEM;

	pskelunit->device = device;
	pskelunit->p_ntp_timestamp_from_counter = p_ntp_timestamp_from_counter;

	*handle = (*p_create_pps_handle)(pskelunit);
	if (*handle) {
		punit = (pps_unit_t *)*handle;
		punit->params.api_version = PPS_API_VERS_1;
		punit->params.mode = PPS_CAPTUREASSERT | PPS_TSFMT_TSPEC;
	}

	return (*handle)
		   ? 0
		   : ENOMEM;
}


/*
 * prov_time_pps_destroy - release PPS handle
 */
int WINAPI
prov_time_pps_destroy(
	pps_unit_t *	unit,
	void *		context
	)
{
	skel_unit *pskelunit;

	UNUSED(unit);

	pskelunit = context;
	free(pskelunit);

	return 0;
}


/*
 * prov_time_pps_setparams - set parameters for handle
 */
int WINAPI
prov_time_pps_setparams(
	pps_unit_t *		unit,
	void *			context,
	const pps_params_t *	params
	)
{
	skel_unit *pskelunit;
	int	mode, mode_in;

	pskelunit = context;

	/*
	 * There was no reasonable consensus in the API working group.
	 * I require `api_version' to be set!
	 */

	if (params->api_version != PPS_API_VERS_1)
		return EINVAL;

	/*
	 * The only settable modes are PPS_CAPTUREASSERT,
	 * PPS_OFFSETASSERT, and the timestamp formats.
	 */

	mode_in = params->mode;

	/*
	 * Only one of the time formats may be selected
	 * if a nonzero assert offset is supplied.
	 */
	if ((mode_in & (PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP))
	    == (PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP)) {

		if (params->assert_offset.tv_sec ||
		    params->assert_offset.tv_nsec) 
			return EINVAL;

		/*
		 * If no offset was specified but both time
		 * format flags are used consider it harmless
		 * but turn off PPS_TSFMT_NTPFP so getparams
		 * will not show both formats lit.
		 */
		mode_in &= ~PPS_TSFMT_NTPFP;
	}

	/* turn off read-only bits */

	mode_in &= ~SKELPPS_RO;

	/*
	 * test remaining bits, should only have captureassert, 
	 * offsetassert, and/or timestamp format bits.
	 */

	if (mode_in & ~(PPS_CAPTUREASSERT | PPS_OFFSETASSERT |
			PPS_TSFMT_TSPEC | PPS_TSFMT_NTPFP))
		return EOPNOTSUPP;

	/*
	 * ok, ready to go.
	 */

	mode = unit->params.mode;
	unit->params = *params;
	unit->params.mode = mode | mode_in;

	return 0;
}


/*
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
	ULONGLONG WindowsTimestamp;
	ULONGLONG Counterstamp;
	skel_unit *pskelunit;
	pps_info_t infobuf;

	/*
	 * nb. PPS_CANWAIT is NOT set by the implementation, we can totally
	 * ignore the timeout variable.
	 */
	UNUSED(timeout);
	pskelunit = context;

	memset(&infobuf, 0, sizeof(infobuf));

	/*
	 * if not captureassert, nothing to return.
	 */

	if (!(unit->params.mode & PPS_CAPTUREASSERT)) {
		*pinfo = infobuf;
		return 0;
	}

	/*
	 * ADD CODE to retrieve timestamp here.
	 */
	WindowsTimestamp = Counterstamp = 0;
	/*
	 * ADD CODE to retrieve timestamp here.
	 */

	/*
	 * pps_ntp_timestamp_from_counter takes the two flavors
	 * of timestamp we have (counter and system time) and
	 * uses whichever it can to give the best NTP fixed-point
	 * conversion.  In ntpd the Counterstamp is typically
	 * used.  A stub implementation in timepps.h simply
	 * converts from Windows timestamp to NTP fixed-point.
	 * We call through a pointer to get ntpd's version.
	 */
	(*pskelunit->p_ntp_timestamp_from_counter)(
		&infobuf.assert_timestamp_ntpfp, 
		WindowsTimestamp, 
		Counterstamp);

	/*
	 * Note that only assert timestamps
	 * are captured by this interface.
	 */
	infobuf.assert_sequence = 0; /* ADD CODE */

	/*
	 * Apply offset and translate to specified format
	 */
	switch (tsformat) {
	case PPS_TSFMT_NTPFP:	/* NTP format requires no translation */
		if (unit->params.mode & PPS_OFFSETASSERT) {
			NTPFP_L_ADDS(&infobuf.assert_timestamp_ntpfp, 
				     &unit->params.assert_offset_ntpfp);
		}
		break;		

	case PPS_TSFMT_TSPEC:	/* timespec format requires conversion to nsecs form */
		PPS_NTPTOTSPEC(infobuf.assert_timestamp);
		if (unit->params.mode & PPS_OFFSETASSERT) {
			infobuf.assert_timestamp.tv_sec  += 
				unit->params.assert_offset.tv_sec;
			infobuf.assert_timestamp.tv_nsec += 
				unit->params.assert_offset.tv_nsec;
			PPS_NORMALIZE(infobuf.assert_timestamp);
		}
		break;

	default:
		return EINVAL;
	}

	infobuf.current_mode = unit->params.mode;
	*pinfo = infobuf;
	return (0);
}


/*
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


/*
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
	ppsapi_provider test_prov;

	if (ppsapi_timepps_prov_ver < PPSAPI_TIMEPPS_PROV_VER)
		return 0;

	p_create_pps_handle = create_pps_handle;
	p_ntp_timestamp_from_counter = ntp_timestamp_from_counter;

	strncpy(short_name_buf, "skeleton", short_name_size);
	strncpy(full_name_buf, 
		"skeleton, ADD CODE to make useful",
		full_name_size);

	/*
	 * Use function pointer prototypes from timepps.h to verify
	 * our prototypes match with some otherwise pointless code.
	 */
	test_prov.ptime_pps_create = &prov_time_pps_create;
	test_prov.ptime_pps_destroy = &prov_time_pps_destroy;
	test_prov.ptime_pps_fetch = &prov_time_pps_fetch;
	test_prov.ptime_pps_kcbind = &prov_time_pps_kcbind;
	test_prov.ptime_pps_setparams = &prov_time_pps_setparams;
	
	return SKELPPS_CAPS;
}
