#	$OpenBSD: Makefile,v 1.2 2001/06/29 07:02:09 markus Exp $

.PATH:		${.CURDIR}/..

CARDLET=	Ssh.bin
DATADIR=	/usr/libdata/ssh

all: ${CARDLET}

clean:
	rm -f ${CARDLET}

install: ${CARDLET}
	install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	    ${CARDLET} ${DESTDIR}${DATADIR}

Ssh.bin: ${.CURDIR}/Ssh.bin.uu
	uudecode ${.CURDIR}/$@.uu

.include <bsd.prog.mk>
