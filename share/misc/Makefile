#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/misc/Makefile,v 1.17 1999/12/23 09:28:49 phk Exp $

FILES=	ascii birthtoken bsd-family-tree eqnchar flowers init.ee \
	iso3166 iso639 man.template mdoc.template operator scsi_modes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
		${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
