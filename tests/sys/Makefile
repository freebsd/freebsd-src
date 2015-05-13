# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR= ${TESTSBASE}/sys

TESTS_SUBDIRS+=		kern
TESTS_SUBDIRS+=		kqueue
TESTS_SUBDIRS+=		netinet

# Items not integrated into kyua runs by default
SUBDIR+=		pjdfstest

.include <bsd.test.mk>
