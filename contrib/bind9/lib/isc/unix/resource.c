/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: resource.c,v 1.11.206.1 2004/03/06 08:15:01 marka Exp $ */

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>	/* Required on some systems for <sys/resource.h>. */
#include <sys/resource.h>

#include <isc/platform.h>
#include <isc/resource.h>
#include <isc/result.h>
#include <isc/util.h>

#include "errno2result.h"

static isc_result_t
resource2rlim(isc_resource_t resource, int *rlim_resource) {
	isc_result_t result = ISC_R_SUCCESS;

	switch (resource) {
	case isc_resource_coresize:
		*rlim_resource = RLIMIT_CORE;
		break;
	case isc_resource_cputime:
		*rlim_resource = RLIMIT_CPU;
		break;		
	case isc_resource_datasize:
		*rlim_resource = RLIMIT_DATA;
		break;		
	case isc_resource_filesize:
		*rlim_resource = RLIMIT_FSIZE;
		break;		
	case isc_resource_lockedmemory:
#ifdef RLIMIT_MEMLOCK
		*rlim_resource = RLIMIT_MEMLOCK;
#else
		result = ISC_R_NOTIMPLEMENTED;
#endif
		break;
	case isc_resource_openfiles:
#ifdef RLIMIT_NOFILE
		*rlim_resource = RLIMIT_NOFILE;
#else
		result = ISC_R_NOTIMPLEMENTED;
#endif
		break;
	case isc_resource_processes:
#ifdef RLIMIT_NPROC
		*rlim_resource = RLIMIT_NPROC;
#else
		result = ISC_R_NOTIMPLEMENTED;
#endif
		break;
	case isc_resource_residentsize:
#ifdef RLIMIT_RSS
		*rlim_resource = RLIMIT_RSS;
#else
		result = ISC_R_NOTIMPLEMENTED;
#endif
		break;
	case isc_resource_stacksize:
		*rlim_resource = RLIMIT_STACK;
		break;
	default:
                /*
		 * This test is not very robust if isc_resource_t
		 * changes, but generates a clear assertion message.
		 */
		REQUIRE(resource >= isc_resource_coresize &&
			resource <= isc_resource_stacksize);

		result = ISC_R_RANGE;
		break;
	}

	return (result);
}

isc_result_t
isc_resource_setlimit(isc_resource_t resource, isc_resourcevalue_t value) {
	struct rlimit rl;
	ISC_PLATFORM_RLIMITTYPE rlim_value;
	int unixresult;
	int unixresource;
	isc_result_t result;

	result = resource2rlim(resource, &unixresource);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (value == ISC_RESOURCE_UNLIMITED)
		rlim_value = RLIM_INFINITY;

	else {
		/*
		 * isc_resourcevalue_t was chosen as an unsigned 64 bit
		 * integer so that it could contain the maximum range of
		 * reasonable values.  Unfortunately, this exceeds the typical
		 * range on Unix systems.  Ensure the range of
		 * ISC_PLATFORM_RLIMITTYPE is not overflowed.
		 */
		isc_resourcevalue_t rlim_max;
		isc_boolean_t rlim_t_is_signed =
			ISC_TF(((double)(ISC_PLATFORM_RLIMITTYPE)-1) < 0);

		if (rlim_t_is_signed)
			rlim_max = ~((ISC_PLATFORM_RLIMITTYPE)1 <<
				     (sizeof(ISC_PLATFORM_RLIMITTYPE) * 8 - 1));
		else
			rlim_max = (ISC_PLATFORM_RLIMITTYPE)-1;

		if (value > rlim_max)
			value = rlim_max;

		rlim_value = value;
	}

	/*
	 * The BIND 8 documentation reports:
	 *
	 *	Note: on some operating systems the server cannot set an
	 *	unlimited value and cannot determine the maximum number of
	 *	open files the kernel can support. On such systems, choosing
	 *	unlimited will cause the server to use the larger of the
	 *	rlim_max for RLIMIT_NOFILE and the value returned by
	 *	sysconf(_SC_OPEN_MAX). If the actual kernel limit is larger
	 *	than this value, use limit files to specify the limit
	 *	explicitly.
	 *
	 * The CHANGES for 8.1.2-T3A also mention:
	 *
	 *	352. [bug] Because of problems with setting an infinite
	 *	rlim_max for RLIMIT_NOFILE on some systems, previous versions
	 *	of the server implemented "limit files unlimited" by setting
	 *	the limit to the value returned by sysconf(_SC_OPEN_MAX).  The
	 *	server will now use RLIM_INFINITY on systems which allow it.
	 *
	 * At some point the BIND 8 server stopped using SC_OPEN_MAX for this
	 * purpose at all, but it isn't clear to me when or why, as my access
	 * to the CVS archive is limited at the time of this writing.  What
	 * BIND 8 *does* do is to set RLIMIT_NOFILE to either RLIMIT_INFINITY
	 * on a half dozen operating systems or to FD_SETSIZE on the rest,
	 * the latter of which is probably fewer than the real limit.  (Note
	 * that libisc's socket module will have problems with any fd over
	 * FD_SETSIZE.  This should be fixed in the socket module, not a
	 * limitation here.  BIND 8's eventlib also has a problem, making
	 * its RLIMIT_INFINITY setting useless, because it closes and ignores
	 * any fd over FD_SETSIZE.)
	 *
	 * More troubling is the reference to some operating systems not being
	 * able to set an unlimited value for the number of open files.  I'd
	 * hate to put in code that is really only there to support archaic
	 * systems that the rest of libisc won't work on anyway.  So what this
	 * extremely verbose comment is here to say is the following:
	 *
	 *   I'm aware there might be an issue with not limiting the value
	 *   for RLIMIT_NOFILE on some systems, but since I don't know yet
	 *   what those systems are and what the best workaround is (use
	 *   sysconf()?  rlim_max from getrlimit()?  FD_SETSIZE?) so nothing
	 *   is currently being done to clamp the value for open files.
	 */

	rl.rlim_cur = rl.rlim_max = rlim_value;
	unixresult = setrlimit(unixresource, &rl);

	if (unixresult == 0)
		return (ISC_R_SUCCESS);
	else
		return (isc__errno2result(errno));
}

isc_result_t
isc_resource_getlimit(isc_resource_t resource, isc_resourcevalue_t *value) {
	int unixresult;
	int unixresource;
	struct rlimit rl;
	isc_result_t result;

	result = resource2rlim(resource, &unixresource);
	if (result == ISC_R_SUCCESS) {
		unixresult = getrlimit(unixresource, &rl);
		INSIST(unixresult == 0);
		*value = rl.rlim_max;
	}

	return (result);
}
