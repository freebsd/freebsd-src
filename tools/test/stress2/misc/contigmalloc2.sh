#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# contigmalloc(9) / contigfree(9) test scenario.
# Regression test for allocations >= 2 GiB.
# "panic: vm_page_insert_after: mpred doesn't precede pindex" seen.
# Fixed by r284207.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -d /usr/src/sys ] || exit 0
builddir=`sysctl kern.version | grep @ | sed 's/.*://'`
[ -d "$builddir" ] && export KERNBUILDDIR=$builddir || exit 0
export SYSDIR=`echo $builddir | sed 's#/sys.*#/sys#'`

. ../default.cfg

odir=`pwd`
dir=/tmp/contigmalloc
rm -rf $dir; mkdir -p $dir
cat > $dir/ctest2.c <<EOF
#include <sys/param.h>
#include <sys/syscall.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TALLOC 1
#define TFREE  2

void *p;
long size;
int n;

void
test(int argc, char *argv[])
{
	long mw;
	int no, ps, res;

	if (argc == 3) {
		no = atoi(argv[1]);
		mw = atol(argv[2]);
	}
	if (argc != 3 || no == 0 || mw == 0)
		errx(1, "Usage: %s <syscall number> <max wired>", argv[0]);

	ps = getpagesize();
	size = mw / 100 * 80 * ps;	/* Use 80% of vm.max_user_wired */
	while (size > 0) {
		res = syscall(no, TALLOC, &p, &size);
		if (res == -1) {
			if (errno != ENOMEM)
				warn("contigmalloc(%lu pages) failed",
				    size);
		} else {
#if defined(TEST)
			fprintf(stderr, "pre contigmalloc(%lu pages): %lu MiB\n",
			    size, size * ps / 1024 / 1024);
#endif
			res = syscall(no, TFREE, &p, &size);
#if defined(TEST)
			fprintf(stderr, "contigfree(%lu pages)\n",
			    size);
#endif
		}
		size /= 2;
	}
}

int
main(int argc, char *argv[])
{
	test(argc, argv);

	return (0);
}

EOF
mycc -o /tmp/ctest2 -Wall -Wextra -O0 -g $dir/ctest2.c || exit 1
rm $dir/ctest2.c

cd $dir
cat > Makefile <<EOF
KMOD= cmalloc2
SRCS= cmalloc2.c

.include <bsd.kmod.mk>
EOF

sed '1,/^EOF2/d' < $odir/$0 > cmalloc2.c
make depend all || exit 1
kldload $dir/cmalloc2.ko || exit 1

cd $odir
mw=$((`sysctl -n vm.max_user_wired` - \
    `sysctl -n vm.stats.vm.v_user_wire_count`)) || exit 1
/tmp/ctest2 `sysctl -n debug.cmalloc_offset` $mw #2>&1 | tail -5
kldunload $dir/cmalloc2.ko
rm -rf $dir /tmp/ctest2
exit 0

EOF2
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

#define TALLOC 1
#define TFREE  2

/*
 * Hook up a syscall for contigmalloc testing.
 */

struct cmalloc_args {
        int a_op;
	void *a_ptr;
	void *a_size;
};

static int
cmalloc(struct thread *td, struct cmalloc_args *uap)
{
	void *p;
	unsigned long size;
	int error;

	error = copyin(uap->a_size, &size, sizeof(size));
	if (error != 0) {
		return (error);
	}
	switch (uap->a_op) {
	case TFREE:
		error = copyin(uap->a_ptr, &p, sizeof(p));
		if (error == 0) {
			if (p != NULL)
				contigfree(p, size, M_TEMP);
		}
		return (error);

	case TALLOC:
		p = contigmalloc(size, M_TEMP, M_NOWAIT, 0ul, ~0ul, 4096, 0);
		if (p != NULL) {
			error = copyout(&p, uap->a_ptr, sizeof(p));
			return (error);
		}
		return (ENOMEM);
	}
        return (EINVAL);
}

/*
 * The sysent for the new syscall
 */
static struct sysent cmalloc_sysent = {
	.sy_narg =  3,				/* sy_narg */
	.sy_call = (sy_call_t *) cmalloc	/* sy_call */
};

/*
 * The offset in sysent where the syscall is allocated.
 */
static int cmalloc_offset = NO_SYSCALL;

SYSCTL_INT(_debug, OID_AUTO, cmalloc_offset, CTLFLAG_RD, &cmalloc_offset, 0,
    "cmalloc syscall number");

/*
 * The function called at load/unload.
 */

static int
cmalloc_load(struct module *module, int cmd, void *arg)
{
        int error = 0;

        switch (cmd) {
        case MOD_LOAD :
                break;
        case MOD_UNLOAD :
                break;
        default :
                error = EOPNOTSUPP;
                break;
        }
        return (error);
}

SYSCALL_MODULE(cmalloc_syscall, &cmalloc_offset, &cmalloc_sysent,
    cmalloc_load, NULL);
