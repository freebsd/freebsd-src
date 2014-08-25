# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/acl
FILESDIR=${TESTSDIR}

FILES+=	acl.cfg
FILES+=	acl_common.kshlib
FILES+=	cleanup.ksh
FILES+=	setup.ksh

TESTS_SUBDIRS+=	cifs
TESTS_SUBDIRS+=	trivial
TESTS_SUBDIRS+=	nontrivial

.include <bsd.test.mk>
