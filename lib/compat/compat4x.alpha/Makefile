# $FreeBSD: src/lib/compat/compat4x.alpha/Makefile,v 1.2.2.2 2002/01/24 19:00:47 obrien Exp $

DISTRIBUTION=	compat4x

LIBCOMPATDIR=	${LIBDIR}/compat

LIBS=	\
	libcrypto.so.1 \
	libfetch.so.2 \
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
