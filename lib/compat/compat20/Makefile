# $Id: Makefile,v 1.3 1996/12/28 17:42:47 peter Exp $

DISTRIBUTION=	compat20

LIBS=	libdialog.so.2.0 libforms.so.2.0 libg++.so.2.0 libgcc.so.261.0 \
	libncurses.so.2.0 libreadline.so.2.0

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
${lib}:	${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gunzip > ${lib}
.endfor

install:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
