# $FreeBSD$

DISTRIBUTION=	compat20

LIBS=	libdialog.so.2.0 libforms.so.2.0 libg++.so.2.0 libgcc.so.261.0 \
	libncurses.so.2.0 libreadline.so.2.0

CLEANFILES+=	${LIBS} ld.so

all:	${LIBS} ld.so

.for lib in ${LIBS} ld.so
${lib}:	${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gunzip > ${lib}
.endfor

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ld.so \
		${DESTDIR}/usr/libexec
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}/aout

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
