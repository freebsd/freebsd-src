#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.9 1997/03/31 20:18:55 andreas Exp $

FILES=	airport ascii birthtoken bsd-family-tree eqnchar flowers inter.phone \
	iso3166 man.template mdoc.template na.phone operator scsi_modes
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	cd ${.CURDIR}; ${INSTALL} ${COPY} -o ${SHAREOWN} -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${FILES} ${DESTDIR}${SHAREDIR}/misc

.include <bsd.prog.mk>
