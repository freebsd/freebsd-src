# $Id$

DISTRIBUTION=	compat21

LIBS=	libc.so.2.2

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
${lib}:	${lib}.gz.uu
	uudecode ${.CURDIR}/${lib}.gz.uu
	gunzip ${lib}.gz
.endfor

install:
	${INSTALL} -p -o ${BINMODE} -g ${BINGRP} -m ${BINMODE} ${LIBS} \
		${DESTDIR}${BINDIR}

# Get all the fruit, even though we don't set PROG
.include <bsd.prog.mk>
