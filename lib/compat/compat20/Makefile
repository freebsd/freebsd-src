# $Id: Makefile,v 1.1 1996/12/23 05:07:32 peter Exp $

DISTRIBUTION=	compat20

LIBS=	libdialog.so.2.0 libforms.so.2.0 libg++.so.2.0 libgcc.so.261.0 \
	libncurses.so.2.0 libreadline.so.2.0

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
${lib}:	${lib}.gz.uu
	uudecode ${.CURDIR}/${lib}.gz.uu
	gunzip ${lib}.gz
.endfor

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ${LIBS} \
		${DESTDIR}${BINDIR}

# Get all the fruit, even though we don't set PROG
.include <bsd.prog.mk>
