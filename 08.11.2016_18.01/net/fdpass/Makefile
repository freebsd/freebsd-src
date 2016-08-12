# $NetBSD: Makefile,v 1.4 2012/09/13 21:13:34 joerg Exp $
#

SHLIBDIR=		/usr/lib
SHLIBINSTALLDIR=	/usr/lib

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/fdpass

TESTS_SH=	t_fdpass

PROGS=          fdpass64 fdpass32
MAN.fdpass64=    # empty
BINDIR.fdpass64= ${TESTSDIR}

.if (${MACHINE} == "sparc64" || ${MACHINE} == "amd64") && ${MKCOMPAT} != "no"
COPTS.fdpass32.c	+= -m32
LDFLAGS.fdpass32	+= -m32
.endif
MAN.fdpass32=    # empty
BINDIR.fdpass32= ${TESTSDIR}


fdpass64.c fdpass32.c:	fdpass.c
	ln -s ${.CURDIR}/fdpass.c ${.TARGET}

CLEANFILES	+= fdpass64.c fdpass32.c

.include <bsd.test.mk>
