#	$NetBSD: Makefile,v 1.2 1995/07/03 21:25:05 cgd Exp $
# $FreeBSD$

LIBS=	llib-lposix.ln llib-lstdc.ln
LINT=	${.OBJDIR}/../xlint/xlint

all:	${LIBS}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${LIBMODE} \
	    ${LIBS} ${DESTDIR}${LINTLIBDIR}

clean cleanall:
	rm -f ${LIBS}

llib-lposix.ln: llib-lposix
	${LINT} -Cposix ${.ALLSRC}

llib-lstdc.ln: llib-lstdc
	${LINT} -Cstdc ${.ALLSRC}

.include <bsd.prog.mk>
