# $Id: Makefile,v 1.7 1997/02/22 14:57:37 peter Exp $

DISTRIBUTION=	compat20

LIBS=	libdialog.so.2.0 libforms.so.2.0 libg++.so.2.0 libgcc.so.261.0 \
	libncurses.so.2.0 libreadline.so.2.0

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
${lib}:	${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gunzip > ${lib}
.endfor

beforeinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}/aout

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
