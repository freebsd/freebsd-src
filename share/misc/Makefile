#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/misc/Makefile,v 1.15.2.1 1999/08/29 16:47:37 peter Exp $

FILES=	ascii birthtoken bsd-family-tree eqnchar flowers init.ee inter.phone \
	iso3166 iso639 man.template mdoc.template operator scsi_modes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
		${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
