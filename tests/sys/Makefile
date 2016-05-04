# $FreeBSD$

TESTSDIR= ${TESTSBASE}/sys

TESTS_SUBDIRS+=		acl
TESTS_SUBDIRS+=		aio
TESTS_SUBDIRS+=		fifo
TESTS_SUBDIRS+=		file
TESTS_SUBDIRS+=		geom
TESTS_SUBDIRS+=		kern
# XXX-BD TLB miss (store) - kernel mode
.if ${MACHINE_CPUARCH} != "mips"
TESTS_SUBDIRS+=		kqueue
.endif
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
