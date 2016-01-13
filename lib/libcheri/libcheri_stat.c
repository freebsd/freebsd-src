/*-
 * Copyright (c) 2013 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/exec.h>
#include <sys/sysctl.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cheri_invoke.h"
#include "libcheri_stat.h"
#include "sandbox.h"

/*
 * The sandbox statistic mechanism tracks active sandbox classes, methods, and
 * objects, in order to provide useful measurement and diagnostic output for
 * procstat(1) and other tools.  The in-memory representation is designed to
 * make external access simple, rather than offer more mature internal
 * representations (for now).  It takes a particular concern with ABI and safe
 * external concurrent access.  All data structures are designed to be
 * 32/64-bit oblivious -- where pointers must be embedded, they should always
 * be stored in 64-bit fields (although they are largely avoided).
 *
 * External access currently occurs via sysctls offered by the kernel using
 * the ps_string mechanism (a data structure at the top of the main process
 * stack with a set of pointers and lengths into user data that the kernel
 * provides access to).  This is similar to the mechanisms used to allow
 * command-line string and ELF auxiliary data export, and in some ways
 * comparable to GDB's use of rtld-internal state in order to provide useful
 * shared-object debugging information.
 *
 * Three memory regions are maintained, one each for classes, methods, and
 * objects.  Each is an array of structures, one per instance of each.  A
 * unique numeric identifier may be set to '0' to indicate an array slot is
 * unused, or to a value provided by the statistics framework.  String names
 * (for classes, methods) or numeric names (for objects) are also provided
 * (but could have collisions, hence unique IDs).  libprocstat(3) provides a
 * simple API to extract the information for display by procstat(1) and other
 * tools.
 *
 * Consumer code -- e.g., libcheri's sandbox mechanism -- must register and
 * deregister classes, methods, and objects using APIs for them to become
 * visible (and disappear).  Statistics are also maintained by callers via
 * APIs.
 *
 * It is easy to imagine numerous enhancements to binary compatibility,
 * flexibility, etc.
 */

/*
 * XXXRW: 640K will be enough for anybody.
 */
#define	SANDBOX_CLASS_STAT_MAX	32
#define	SANDBOX_METHOD_STAT_MAX	256
#define	SANDBOX_OBJECT_STAT_MAX	256

/*
 * Synchronisation in this file is slightly obscure.  From an in-process
 * perspective, it is all about this one lock -- and it is the responsibility
 * of the consumer to not deregister sandboxes or methods prematurely, or more
 * than once.
 *
 * However, we are producing data structures that will be read from outside
 * the process, not just from within.  Sandbox and method IDs of 0 mean
 * unused, so ensure any data structure in an inconsistently allocated or
 * freed state always has an ID of zero, and do this with acquire/release
 * semantics to ensure the 0 is set last during registration, and first during
 * deregistration.  This should minimise confusion in consuming applications,
 * which may see inconsistency (due to racing with libcheri) but at least
 * won't see pre-registration/post-deregistration values.
 *
 * XXXRW: Statistics are updated without the lock -- currently with no
 * synchronisation, but atomic operations could easily be used.  See the
 * macros in libprocstat/sandbox_stat.h.
 *
 * XXXRW: Should we do the thing where we put a generation counter at both the
 * beginning and end of the structure so that a consumer can detect an
 * inconsistent snapshot...?
 */

static pthread_mutex_t sandbox_stat_mtx = PTHREAD_MUTEX_INITIALIZER;

/* Begin allocating IDs at 1 so that 0 can mean unallocated. */
static uint64_t	next_classid = (SANDBOX_CLASSID_FREE + 1);
static uint64_t	next_methodid = (SANDBOX_METHODID_FREE + 1);
static uint64_t next_objectid = (SANDBOX_OBJECTID_FREE + 1);

static struct sandbox_class_stat sandbox_class_stats[SANDBOX_CLASS_STAT_MAX];
static struct sandbox_method_stat
    sandbox_method_stats[SANDBOX_METHOD_STAT_MAX];
static struct sandbox_object_stat
    sandbox_object_stats[SANDBOX_OBJECT_STAT_MAX];

__attribute__ ((constructor)) static void
sandbox_stat_init(void)
{
	struct ps_strings *ps_strings;
	unsigned long ul_ps_strings;
	size_t len;

	len = sizeof(ul_ps_strings);
	if (sysctlbyname("kern.ps_strings", &ul_ps_strings, &len, NULL, 0)
	    == -1)
		return;

	/*
	 * NB: remote-process consumers attaching early should be careful to
	 * check both the pointer and length for non-zero values before
	 * assuming that dereferencing is OK.
	 *
	 * XXXRW: It would be nice if these pointer assignments had release
	 * semantics.
	 */
#ifdef __CHERI_SANDBOX__
	ps_strings = (struct ps_strings *)cheri_setoffset(cheri_getdefault(),
	    ul_ps_strings);
#else
	ps_strings = (struct ps_strings *)ul_ps_strings;
#endif
	ps_strings->ps_sbclasseslen = sizeof(sandbox_class_stats);
	ps_strings->ps_sbclasses = sandbox_class_stats;
	ps_strings->ps_sbmethodslen = sizeof(sandbox_method_stats);
	ps_strings->ps_sbmethods = sandbox_method_stats;
	ps_strings->ps_sbobjectslen = sizeof(sandbox_object_stats);
	ps_strings->ps_sbobjects = sandbox_object_stats;
}

/*
 * Sandbox class routines.
 */
int
sandbox_stat_class_register(struct sandbox_class_stat **scspp,
    const char *name)
{
	struct sandbox_class_stat *scsp;
	u_int i;

	(void)pthread_mutex_lock(&sandbox_stat_mtx);
	for (i = 0; i < SANDBOX_CLASS_STAT_MAX; i++) {
		if (sandbox_class_stats[i].scs_classid ==
		    SANDBOX_CLASSID_FREE)
			break;
	}
	if (i == SANDBOX_CLASS_STAT_MAX) {
		(void)pthread_mutex_unlock(&sandbox_stat_mtx);
		*scspp = NULL;
		errno = ENOMEM;
		return (-1);
	}
	scsp = &sandbox_class_stats[i];
	(void)strlcpy(scsp->scs_class_name, name,
	    sizeof(scsp->scs_class_name));
	scsp->scs_classid = next_classid;
	next_classid++;
	(void)pthread_mutex_unlock(&sandbox_stat_mtx);
	*scspp = scsp;
	return (0);
}

void
sandbox_stat_class_deregister(struct sandbox_class_stat *scsp)
{

	(void)pthread_mutex_lock(&sandbox_stat_mtx);
	scsp->scs_classid = SANDBOX_CLASSID_FREE;
	memset(scsp, '\0', sizeof(*scsp));
	(void)pthread_mutex_unlock(&sandbox_stat_mtx);
}

/*
 * Sandbox method routines.
 */
int
sandbox_stat_method_register(struct sandbox_method_stat **smspp,
    struct sandbox_class_stat *scsp, const char *name)
{
	struct sandbox_method_stat *smsp;
	u_int i;

	(void)pthread_mutex_lock(&sandbox_stat_mtx);
	for (i = 0; i < SANDBOX_METHOD_STAT_MAX; i++) {
		if (sandbox_method_stats[i].sms_methodid ==
		    SANDBOX_METHODID_FREE)
			break;
	}
	if (i == SANDBOX_METHOD_STAT_MAX) {
		(void)pthread_mutex_unlock(&sandbox_stat_mtx);
		*smspp = NULL;
		errno = ENOMEM;
		return (-1);
	}
	smsp = &sandbox_method_stats[i];
	strlcpy(smsp->sms_class_name, scsp->scs_class_name,
	    sizeof(smsp->sms_class_name));
	smsp->sms_classid = scsp->scs_classid;
	(void)strlcpy(smsp->sms_method_name, name,
	    sizeof(smsp->sms_method_name));
	smsp->sms_methodid = next_methodid;
	next_methodid++;
	(void)pthread_mutex_unlock(&sandbox_stat_mtx);
	*smspp = smsp;
	return (0);
}

void
sandbox_stat_method_deregister(struct sandbox_method_stat *smsp)
{

	(void)pthread_mutex_lock(&sandbox_stat_mtx);
	smsp->sms_methodid = SANDBOX_METHODID_FREE;
	memset(smsp, '\0', sizeof(*smsp));
	(void)pthread_mutex_unlock(&sandbox_stat_mtx);
}

/*
 * Sandbox object routines.
 */
int
sandbox_stat_object_register(struct sandbox_object_stat **sospp,
    struct sandbox_class_stat *scsp, uint64_t type, uint64_t name)
{
	struct sandbox_object_stat *sosp;
	u_int i;

	(void)pthread_mutex_lock(&sandbox_stat_mtx);
	for (i = 0; i < SANDBOX_OBJECT_STAT_MAX; i++) {
		if (sandbox_object_stats[i].sos_objectid ==
		    SANDBOX_OBJECTID_FREE)
			break;
	}
	if (i == SANDBOX_OBJECT_STAT_MAX) {
		(void)pthread_mutex_unlock(&sandbox_stat_mtx);
		*sospp = NULL;
		errno = ENOMEM;
		return (-1);
	}
	sosp = &sandbox_object_stats[i];
	strlcpy(sosp->sos_class_name, scsp->scs_class_name,
	    sizeof(sosp->sos_class_name));
	sosp->sos_classid = scsp->scs_classid;
	sosp->sos_object_type = type;
	sosp->sos_object_name = name;
	sosp->sos_objectid = next_objectid;
	next_objectid++;
	(void)pthread_mutex_unlock(&sandbox_stat_mtx);
	*sospp = sosp;
	return (0);
}

void
sandbox_stat_object_deregister(struct sandbox_object_stat *sosp)
{

	(void)pthread_mutex_lock(&sandbox_stat_mtx);
	sosp->sos_objectid = SANDBOX_OBJECTID_FREE;
	memset(sosp, '\0', sizeof(*sosp));
	(void)pthread_mutex_unlock(&sandbox_stat_mtx);
}
