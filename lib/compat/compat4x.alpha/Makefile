# $FreeBSD$

DISTRIBUTION=	compat4x

LIBCOMPATDIR=	${LIBDIR}/compat

LIBS=	\
	libc.so.4 \
	libc_r.so.4 \
	libcrypto.so.1 \
	libfetch.so.2 \
	libedit.so.3 \
	libperl.so.3 \
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
