#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.5 1996/09/22 21:54:19 wosch Exp $

PROG=	rbootd
SRCS=	bpf.c conf.c parseconf.c rbootd.c rmpproto.c utils.c
MAN8=	rbootd.8

# XXX BROKEN:	afterinstall:
XXXafterinstall:
	(cd ${.CURDIR}/bootdir && ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} \
	    -m 444 * ${DESTDIR}/usr/mdec/)

.include <bsd.prog.mk>
