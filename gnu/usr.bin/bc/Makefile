# $FreeBSD: src/gnu/usr.bin/bc/Makefile,v 1.17 2000/01/10 08:55:02 phantom Exp $

.PATH:	${.CURDIR}/../../../contrib/bc/bc \
	${.CURDIR}/../../../contrib/bc/lib \
	${.CURDIR}/../../../contrib/bc/doc

PROG=	bc
SRCS=	bc.y execute.c global.c load.c main.c scan.l storage.c util.c \
	number.c getopt.c getopt1.c
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../../contrib/bc/h -DHAVE_CONFIG_H
DPADD=	${LIBREADLINE} ${LIBTERMCAP}
LDADD=	-lreadline -ltermcap

EXAMPLES=	ckbook.b pi.b primes.b twins.b
EXAMPDIR=	/usr/share/examples/bc

beforeinstall:
.for file in ${EXAMPLES}
	cd ${.CURDIR}/../../../contrib/bc/Examples ; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${file} \
		${DESTDIR}${EXAMPDIR}
.endfor

.include <bsd.prog.mk>
