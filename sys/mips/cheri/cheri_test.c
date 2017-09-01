/*-
 * Copyright (c) 2016 Robert N. M. Watson
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/cheri_serial.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <machine/atomic.h>
#include <machine/cherireg.h>
#include <machine/pcb.h>
#include <machine/proc.h>
#include <machine/sysarch.h>

/*
 * Various test cases that can be exercised in a CHERI-aware kernel using
 * sysctl.  They currently assume that the compiler used for the kernel is not
 * CHERI-aware, and so trigger cases via inline assembly.
 */

static int	cheri_test_sysctl_nulldereference(SYSCTL_HANDLER_ARGS);
static int	cheri_test_sysctl_fineload(SYSCTL_HANDLER_ARGS);
static int	cheri_test_sysctl_finestore(SYSCTL_HANDLER_ARGS);
static int	cheri_test_sysctl_tagviolation(SYSCTL_HANDLER_ARGS);
static int	cheri_test_sysctl_boundsviolation(SYSCTL_HANDLER_ARGS);
static int	cheri_test_sysctl_permsviolation(SYSCTL_HANDLER_ARGS);

SYSCTL_NODE(_security_cheri, OID_AUTO, test, CTLFLAG_RD, 0,
    "CHERI test cases");

SYSCTL_PROC(_security_cheri_test, OID_AUTO, null_dereference,
    CTLTYPE_INT | CTLFLAG_RW, NULL, 0, cheri_test_sysctl_nulldereference, "I",
    "Set to trigger a kernel NULL capability dereference");

SYSCTL_PROC(_security_cheri_test, OID_AUTO, fine_load,
    CTLTYPE_INT | CTLFLAG_RW, NULL, 0, cheri_test_sysctl_fineload, "I",
    "Set to trigger a valid load via capability");

SYSCTL_PROC(_security_cheri_test, OID_AUTO, fine_store,
    CTLTYPE_INT | CTLFLAG_RW, NULL, 0, cheri_test_sysctl_finestore, "I",
    "Set to trigger a valid store via capability");

SYSCTL_PROC(_security_cheri_test, OID_AUTO, tag_violation,
    CTLTYPE_INT | CTLFLAG_RW, NULL, 0, cheri_test_sysctl_tagviolation, "I",
    "Set to trigger a kernel tag violation");

SYSCTL_PROC(_security_cheri_test, OID_AUTO, bounds_violation,
    CTLTYPE_INT | CTLFLAG_RW, NULL, 0, cheri_test_sysctl_boundsviolation, "I",
    "Set to trigger a kernel bounds violation");

SYSCTL_PROC(_security_cheri_test, OID_AUTO, perms_violation,
    CTLTYPE_INT | CTLFLAG_RW, NULL, 0, cheri_test_sysctl_permsviolation, "I",
    "Set to trigger a kernel permissions violation");

/*
 * Integer target for various successful (and failing) capability test
 * operations.
 */
static int	cheri_test_int;

/*
 * Various pre-initialised capabilities to test with.
 */
static void * __capability	cheri_test_nullcap;
static void * __capability	cheri_test_finecap;
static void * __capability	cheri_test_untaggedcap;
static void * __capability	cheri_test_nilboundscap;
static void * __capability	cheri_test_readonlycap;

static void
cheri_test_init(void)
{

	/* NULL capability. */
	cheri_test_nullcap = NULL;

	/*
	 * Valid capability to cheri_test_int -- which should be read-write.
	 * Most other test caps are derived from this one.
	 */
	cheri_capability_set(&cheri_test_finecap,
	    CHERI_PERM_LOAD | CHERI_PERM_STORE, (vaddr_t)&cheri_test_int,
	    sizeof(cheri_test_int), 0);

	/* Valid capability to cheri_test_int -- but tag stripped. */
	cheri_test_untaggedcap = cheri_cleartag(cheri_test_finecap);

	/* Valid capability to cheri_test_int -- but bounds set to 0 length. */
	cheri_test_nilboundscap = cheri_csetbounds(cheri_test_finecap, 0);

	/* Valid capability to cheri_test_int -- but read-only. */
	cheri_test_readonlycap = cheri_andperm(cheri_test_finecap,
	    CHERI_PERM_LOAD);
}
SYSINIT(cheri_test_init, SI_SUB_CPU, SI_ORDER_ANY, cheri_test_init, NULL);

/*
 * Sysctls for various operations -- performmed only if a write is made to the
 * MIB, in order to avoid accidental execution (e.g., via "sysctl -a").
 */
static int
cheri_test_sysctl_nulldereference(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &cheri_test_nullcap, 0);
	CHERI_CSW(i, 0, 0, CHERI_CR_CTEMP0);
	return (0);
}

static int
cheri_test_sysctl_fineload(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &cheri_test_finecap, 0);
	CHERI_CLW(i, 0, 0, CHERI_CR_CTEMP0);
	return (0);
}

static int
cheri_test_sysctl_finestore(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &cheri_test_finecap, 0);
	CHERI_CSW(i, 0, 0, CHERI_CR_CTEMP0);
	return (0);
}

static int
cheri_test_sysctl_tagviolation(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &cheri_test_untaggedcap, 0);
	CHERI_CSW(i, 0, 0, CHERI_CR_CTEMP0);
	return (0);
}

static int
cheri_test_sysctl_boundsviolation(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &cheri_test_nilboundscap, 0);
	CHERI_CSW(i, 0, 0, CHERI_CR_CTEMP0);
	return (0);
}

static int
cheri_test_sysctl_permsviolation(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &cheri_test_readonlycap, 0);
	CHERI_CSW(i, 0, 0, CHERI_CR_CTEMP0);
	return (0);
}
