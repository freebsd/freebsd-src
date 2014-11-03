# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/..

TESTSDIR= ${TESTSBASE}/sys

TESTS_SUBDIRS+=		kern
TESTS_SUBDIRS+=		netinet

# Items not integrated into kyua runs by default
SUBDIR+=		pjdfstest

KYUAFILE= yes

.include <bsd.test.mk>
