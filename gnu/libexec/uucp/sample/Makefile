# $Id: Makefile,v 1.5 1997/02/22 15:28:24 peter Exp $

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
