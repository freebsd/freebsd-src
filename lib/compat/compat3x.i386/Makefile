# $FreeBSD: src/lib/compat/compat3x.i386/Makefile,v 1.8.2.1 2000/06/14 14:58:49 ache Exp $

DISTRIBUTION=	compat3x

LIBCOMPATDIR=	${LIBDIR}/compat

LIBS=	libc.so.3 libc_r.so.3 libcurses.so.2 libdialog.so.3 libedit.so.2 \
	libf2c.so.2 libftpio.so.4 libg++.so.4 libhistory.so.3 libmytinfo.so.2 \
	libncurses.so.3 libreadline.so.3 libstdc++.so.2 libtermcap.so.2 \
	libutil.so.2 libwrap.so.2 libxpg4.so.2

CLEANFILES+=	${LIBS}

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
