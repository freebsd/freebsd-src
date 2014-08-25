# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/zvol
FILESDIR=${TESTSDIR}

FILES+=	zvol.cfg
FILES+=	zvol_common.kshlib

TESTS_SUBDIRS+=	zvol_ENOSPC
TESTS_SUBDIRS+=	zvol_swap
TESTS_SUBDIRS+=	zvol_misc
TESTS_SUBDIRS+=	zvol_cli

.include <bsd.test.mk>
