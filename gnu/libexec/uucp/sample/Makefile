# $Id: Makefile,v 1.2 1994/04/25 16:17:59 jkh Exp $

FILES=	call.sample config.sample dial.sample dialcode.sample passwd.sample \
	port.sample sys1.sample sys2.sample

NOOBJ=	noobj

BINOWN=	$(owner)
BINGRP=	$(group)

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 440 ${FILES} \
		${DESTDIR}/etc/uucp

.include <bsd.prog.mk>
