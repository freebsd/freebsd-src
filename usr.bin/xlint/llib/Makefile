#	$NetBSD: Makefile,v 1.7 2000/06/14 20:22:19 matt Exp $
# $FreeBSD$

.include <bsd.own.mk>

LIBS=	llib-lposix.ln llib-lstdc.ln
LINT=	${.OBJDIR}/../xlint/xlint

FILES=	${LIBS}
FILESDIR= ${LINTLIBDIR}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${LIBMODE} \
	    ${LIBS} ${DESTDIR}${LINTLIBDIR}

CLEANFILES+= ${LIBS}

llib-lposix.ln: llib-lposix
	${LINT} ${LINTFLAGS} -Cposix ${.ALLSRC}

llib-lstdc.ln: llib-lstdc
	${LINT} ${LINTFLAGS} -Cstdc ${.ALLSRC}

.include <bsd.prog.mk>
