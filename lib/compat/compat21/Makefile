# $Id: Makefile,v 1.1.2.1 1996/12/23 21:47:59 jkh Exp $

DISTRIBUTION=	compat21

LIBS=	libc.so.2.2 libgmp.so.2.0

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
${lib}:	${lib}.gz.uu
	uudecode ${.CURDIR}/${lib}.gz.uu
	gunzip ${lib}.gz
.endfor

install:
	${INSTALL} -p -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ${LIBS} \
		${DESTDIR}${BINDIR}

# Get all the fruit, even though we don't set PROG
.include <bsd.prog.mk>
