#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.12 1998/04/11 20:45:14 phk Exp $

FILES=	ascii birthtoken bsd-family-tree eqnchar flowers inter.phone \
	iso3166 man.template mdoc.template operator scsi_modes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
		${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
