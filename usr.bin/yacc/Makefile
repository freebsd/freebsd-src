#	@(#)Makefile	5.3 (Berkeley) 5/12/90
# $FreeBSD: src/usr.bin/yacc/Makefile,v 1.7.2.1 1999/08/29 15:35:03 peter Exp $

PROG=	yacc
SRCS=	closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	skeleton.c symtab.c verbose.c warshall.c
MAN1=	yacc.1 yyfix.1
LINKS+=	${BINDIR}/yacc ${BINDIR}/byacc
MLINKS+=yacc.1 byacc.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/yyfix.sh ${DESTDIR}${BINDIR}/yyfix

.include <bsd.prog.mk>
