# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR= ${TESTSBASE}/sys

TESTS_SUBDIRS+=		kern
TESTS_SUBDIRS+=		netinet
TESTS_SUBDIRS+=		opencrypto
TESTS_SUBDIRS+=		sockets

# Items not integrated into kyua runs by default
SUBDIR+=		pjdfstest

.include <bsd.test.mk>
