# $Id: Makefile,v 1.2.6.1 1995/09/01 06:05:06 davidg Exp $

FILES=	call.sample config.sample dial.sample dialcode.sample passwd.sample \
	port.sample sys1.sample sys2.sample

NOOBJ=	noobj

BINOWN=	$(owner)
BINGRP=	$(group)

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 440 ${FILES} \
		${DESTDIR}/etc/uucp

.include <bsd.prog.mk>
