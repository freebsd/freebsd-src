# $FreeBSD: src/lib/compat/compat4x.i386/Makefile,v 1.3.2.1 2001/03/22 01:43:43 ps Exp $

DISTRIBUTION=	compat4x

LIBCOMPATDIR=	${LIBDIR}/compat

LIBS=	\
	libcrypto.so.1 \
	libssl.so.1

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
