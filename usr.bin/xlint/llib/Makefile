#	$NetBSD: Makefile,v 1.2 1995/07/03 21:25:05 cgd Exp $

#LIBS=	llib-lposix.ln llib-lstdc.ln
LIBS=	llib-lstdc.ln

all:	${LIBS}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${LIBMODE} \
	    ${LIBS} ${DESTDIR}${LINTLIBDIR}

clean cleanall:
	rm -f ${LIBS}

llib-lposix.ln: llib-lposix
	lint -Cposix ${.ALLSRC}

llib-lstdc.ln: llib-lstdc
	lint -Cc ${.ALLSRC}

.include <bsd.prog.mk>
