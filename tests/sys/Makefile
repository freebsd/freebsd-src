# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR= ${TESTSBASE}/sys

TESTS_SUBDIRS+=		aio
TESTS_SUBDIRS+=		fifo
TESTS_SUBDIRS+=		file
TESTS_SUBDIRS+=		kern
TESTS_SUBDIRS+=		kqueue
TESTS_SUBDIRS+=		mqueue
TESTS_SUBDIRS+=		netinet
TESTS_SUBDIRS+=		opencrypto
TESTS_SUBDIRS+=		posixshm
TESTS_SUBDIRS+=		vfs
TESTS_SUBDIRS+=		vm

# Items not integrated into kyua runs by default
SUBDIR+=		pjdfstest

SUBDIR_PARALLEL=

.include <bsd.test.mk>
