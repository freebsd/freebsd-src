#	$NetBSD: Makefile,v 1.7 2000/06/14 20:22:19 matt Exp $
# $FreeBSD$

LIBS=	llib-lposix.ln llib-lstdc.ln

FILES=	${LIBS}
FILESDIR= ${LINTLIBDIR}

CLEANFILES+= ${LIBS}

llib-lposix.ln: llib-lposix
	${LINT} ${LINTFLAGS} -Cposix ${.ALLSRC}

llib-lstdc.ln: llib-lstdc
	${LINT} ${LINTFLAGS} -Cstdc ${.ALLSRC}

.include <bsd.prog.mk>

.if !empty(STAGE_INCLUDEDIR)
LINTFLAGS+= -I${STAGE_INCLUDEDIR}
.else
.if ${CFLAGS:M--sysroot=*} != ""
_sysroot?=  ${CFLAGS:M--sysroot=*:[1]:C,^--sysroot=,,}
.elif ${CC:M--sysroot=*} != ""
_sysroot?=  ${CC:M--sysroot=*:[1]:C,^--sysroot=,,}
.endif
.if !empty(_sysroot)
LINTFLAGS+= -I${_sysroot}/usr/include
.endif
.endif
