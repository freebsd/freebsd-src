# $FreeBSD$

TESTSDIR=		${TESTSBASE}/sys

TESTS_SUBDIRS+=		acl
TESTS_SUBDIRS+=		aio
TESTS_SUBDIRS+=		audit
TESTS_SUBDIRS+=		capsicum
TESTS_SUBDIRS+=		fifo
TESTS_SUBDIRS+=		file
TESTS_SUBDIRS+=		fs
TESTS_SUBDIRS+=		geom
TESTS_SUBDIRS+=		kern
TESTS_SUBDIRS+=		kqueue
TESTS_SUBDIRS+=		mac
TESTS_SUBDIRS+=		mqueue
TESTS_SUBDIRS+=		netinet
TESTS_SUBDIRS+=		opencrypto
TESTS_SUBDIRS+=		posixshm
TESTS_SUBDIRS+=		sys
TESTS_SUBDIRS+=		vfs
TESTS_SUBDIRS+=		vm

# Items not integrated into kyua runs by default
SUBDIR+=		pjdfstest

.include <bsd.test.mk>
