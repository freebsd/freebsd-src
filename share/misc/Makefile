#	@(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	airport ascii birthtoken eqnchar flowers inter.phone \
	man.template \
	mdoc.template na.phone operator scsi_modes zipcodes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	cd ${.CURDIR}; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
