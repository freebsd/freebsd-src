# $FreeBSD$

.include <bsd.own.mk>

TESTS_SUBDIRS+=	common

.if ${MACHINE_CPUARCH} == "i386"
TESTS_SUBDIRS+= i386
.endif

.PATH:		${SRCTOP}/tests
KYUAFILE=	yes

.PATH:		${.CURDIR}/tools
SCRIPTSDIR=	${TESTSDIR}
SCRIPTS=	dtest.sh

.include <bsd.test.mk>
