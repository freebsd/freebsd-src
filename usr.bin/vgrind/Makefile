#	@(#)Makefile	5.8 (Berkeley) 6/5/90

PROG=	vfontedpr
SRCS=	regexp.c vfontedpr.c vgrindefs.c
MAN1=	vgrind.1
MAN5=	vgrindefs.5
BINDIR=	/usr/libexec

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/vgrind.sh ${DESTDIR}/usr/bin/vgrind
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/vgrindefs.src \
	    ${DESTDIR}/usr/share/misc/vgrindefs
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/tmac.vgrind \
	    ${DESTDIR}/usr/share/tmac

.include <bsd.prog.mk>
