# $FreeBSD$

DISTRIBUTION=	compat3x

LIBCOMPATDIR=	${LIBDIR}/compat

LIBS=	libc.so.3.gz.uu libc_r.so.3.gz.uu libcurses.so.2.gz.uu \
	libdialog.so.3.gz.uu libedit.so.2.gz.uu libf2c.so.2.gz.uu \
	libftpio.so.4.gz.uu libg++.so.4.gz.uu libmytinfo.so.2.gz.uu \
	libncurses.so.3.gz.uu libreadline.so.3.gz.uu libstdc++.so.2.gz.uu \
	libtermcap.so.2.gz.uu

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
