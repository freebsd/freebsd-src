/*-
 * Copyright 2008 John Birrell <jb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

SDT_PROVIDER_DEFINE(test);

SDT_PROBE_DEFINE7(test, , , sdttest, "int", "int", "int", "int", "int",
    "int", "int");

/*
 * These are variables that the DTrace test suite references in the
 * Solaris kernel. We define them here so that the tests function 
 * unaltered.
 */
int	kmem_flags;

typedef struct vnode vnode_t;
vnode_t dummy;
vnode_t *rootvp = &dummy;

enum argtest {
	ARGTEST_SDT,
	ARGTEST_FBT,
};

extern void fbttest(int, int, int, int, int, int, int, int, int, int);

void __noinline
fbttest(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j)
{
	printf("fbttest(%d, %d, %d, %d, %d, %d, %d, %d, %d, %d)\n",
	    a, b, c, d, e, f, g, h, i, j);
}

/*
 * Test SDT probes with more than 5 arguments. On amd64, such probes require
 * special handling since only the first 5 arguments will be passed to
 * dtrace_probe() in registers; the rest must be fetched off the stack.
 */
static int
dtrace_test_argtest(SYSCTL_HANDLER_ARGS)
{
	int val, error;

	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	else if (val == 0)
		return (0);

	if (arg2 == ARGTEST_SDT)
		SDT_PROBE7(test, , , sdttest, 1, 2, 3, 4, 5, 6, 7);
	else
		fbttest(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

	return (error);
}

static SYSCTL_NODE(_debug, OID_AUTO, dtracetest,
    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "");

SYSCTL_PROC(_debug_dtracetest, OID_AUTO, sdttest,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RW, NULL, ARGTEST_SDT,
    dtrace_test_argtest,
    "I", "Trigger the SDT test probe");
SYSCTL_PROC(_debug_dtracetest, OID_AUTO, fbttest,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RW, NULL, ARGTEST_FBT,
    dtrace_test_argtest,
    "I", "Trigger the FBT test probe");

static int
dtrace_test_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

DEV_MODULE(dtrace_test, dtrace_test_modevent, NULL);
MODULE_VERSION(dtrace_test, 1);
