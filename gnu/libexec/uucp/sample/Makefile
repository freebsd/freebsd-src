# $Id: Makefile,v 1.1 1993/08/05 18:22:22 conklin Exp $

FILES=	call config dial dialcode passwd port sys1 sys2
NOOBJ=	noobj

BINOWN=	$(owner)
BINGRP=	$(group)

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 440 ${FILES} \
		${DESTDIR}/etc/uucp

.include <bsd.prog.mk>
