#	from: @(#)Makefile	5.9 (Berkeley) 7/1/91
#	$Id: Makefile,v 1.7 1993/10/25 21:08:42 rgrimes Exp $

SUBDIR=	man1 man3 man4 man5 man7 man8

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 man0/COPYRIGHT \
	    ${DESTDIR}/usr/share/man/COPYRIGHT

makedb:
	makewhatis ${DESTDIR}${BINDIR}/man

.include <bsd.subdir.mk>
.include "${.CURDIR}/../Makefile.inc"
