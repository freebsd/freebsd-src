# $FreeBSD$

DISTRIBUTION=	compat4x

LIBCOMPATDIR=	${LIBDIR}/compat

LIBS=	\
	libc.so.4 \
	libc_r.so.4 \
	libdialog.so.4 \
	libedit.so.3 \
	libfetch.so.2 \
	libftpio.so.5 \
	libhistory.so.4 \
	libncurses.so.5 \
	libreadline.so.4 \
	libstdc++.so.3 \
	libutil.so.3 \
	libwrap.so.3 \
	libxpg4.so.3

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
