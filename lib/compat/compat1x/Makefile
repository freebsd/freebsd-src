# $Id: Makefile,v 1.4 1996/12/30 07:31:43 jkh Exp $

DISTRIBUTION=	compat1x

LIBS=	libc.so.1.1 libcurses.so.1.1 libf2c.so.1.1 libg++.so.1.1 \
	libgcc.so.1.1 libgnumalloc.so.1.1 libgnuregex.so.1.1 libln.so.1.1 \
	libm.so.1.1 libmalloc.so.1.1 libreadline.so.1.1 libresolv.so.1.1 \
	librpcsvc.so.1.1 libskey.so.1.1 libtelnet.so.1.1 libtermcap.so.1.1 \
	libutil.so.1.1 liby.so.1.1

CLEANFILES+=	${LIBS}
LINKS=	${LIBCOMPATDIR}/libtermcap.so.1.1 ${LIBCOMPATDIR}/libtermlib.so.1.1

all:	${LIBS}

.for lib in ${LIBS}
${lib}:	${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gunzip > ${lib}
.endfor

beforeinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
