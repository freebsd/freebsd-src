# $FreeBSD: src/lib/compat/compat4x.alpha/Makefile,v 1.2.2.5 2003/02/18 22:16:58 nectar Exp $

DISTRIBUTION=	compat4x

LIBS=	\
	libcrypto.so.1 \
	libcrypto.so.2 \
	libfetch.so.2 \
	libssl.so.1 \
	libssl.so.2

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
${lib}:	${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gunzip > ${lib}
.endfor

beforeinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
