#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.6 1996/08/05 18:35:33 wollman Exp $

FILES=	airport ascii birthtoken bsd-family-tree eqnchar flowers inter.phone
	iso3166 man.template mdoc.template na.phone operator scsi_modes \
	zipcodes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	cd ${.CURDIR}; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
