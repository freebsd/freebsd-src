# $FreeBSD$

DISTRIBUTION=	compat4x

LIBS=	\
	libc.so.4 \
	libc_r.so.4 \
	libcrypto.so.1 \
	libedit.so.3 \
	libfetch.so.2 \
	libperl.so.3 \
	libssl.so.1 \
	libusb.so.0

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
.if exists(${.CURDIR}/${lib}.gz.uu)
${lib}:	${.CURDIR}/${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gzip -d > ${lib}
.elif exists(${.CURDIR}/${lib}.bz2.uu)
${lib}:	${.CURDIR}/${lib}.bz2.uu
	uudecode -p ${.CURDIR}/${lib}.bz2.uu | bzip2 -d > ${lib}
.endif
.endfor

beforeinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
