# $FreeBSD: src/gnu/libexec/uucp/sample/Makefile,v 1.5.2.1 1999/08/29 14:30:22 peter Exp $

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
