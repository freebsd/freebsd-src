#	$NetBSD: Makefile,v 1.2 1995/07/03 21:25:05 cgd Exp $
# $FreeBSD: src/usr.bin/xlint/llib/Makefile,v 1.7.2.1 2002/08/07 16:32:02 ru Exp $

LIBS=	llib-lposix.ln llib-lstdc.ln

all:	${LIBS}

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${LIBMODE} \
	    ${LIBS} ${DESTDIR}${LINTLIBDIR}

clean cleanall:
	rm -f ${LIBS}

llib-lposix.ln: llib-lposix
	lint -Cposix ${.ALLSRC}

llib-lstdc.ln: llib-lstdc
	lint -Cstdc ${.ALLSRC}

.include <bsd.prog.mk>
