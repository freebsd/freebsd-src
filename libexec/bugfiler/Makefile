#	from: @(#)Makefile	5.17 (Berkeley) 5/24/90
#	$Id: Makefile,v 1.5 1993/10/20 22:32:32 rgrimes Exp $

PROG=	bugfiler
CFLAGS+=-I${.CURDIR} -DBUGS_HOME=\"owner-FreeBSD-bugfiler@freefall.cdrom.com\"
SRCS=	bugfiler.c error.c gethead.c process.c redist.c reply.c
BINOWN=	root
BINMODE=4555
MAN1=	sendbug.1
MAN8=	bugfiler.8

beforeinstall:
	install -c -o bin -g ${BINGRP} -m 555 \
	    ${.CURDIR}/sendbug.sh ${DESTDIR}/usr/bin/sendbug
	install -c -o bin -g ${BINGRP} -m 444 ${.CURDIR}/bugformat \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
