#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.10 1998/01/01 14:05:30 wosch Exp $

FILES=	airport ascii birthtoken bsd-family-tree eqnchar flowers inter.phone \
	iso3166 man.template mdoc.template na.phone operator scsi_modes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
		${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
