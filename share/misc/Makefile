#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

FILES=	ascii birthtoken bsd-family-tree eqnchar flowers init.ee \
	iso3166 iso639 man.template mdoc.template operator pci_vendors \
	scsi_modes usb_hid_usages
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
		${FILES} ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
