#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.14 1998/09/11 16:38:20 dt Exp $

FILES=	ascii birthtoken bsd-family-tree eqnchar flowers init.ee inter.phone \
	iso3166 iso639 man.template mdoc.template operator scsi_modes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
		${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
