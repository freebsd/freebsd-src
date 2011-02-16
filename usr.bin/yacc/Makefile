#	@(#)Makefile	5.3 (Berkeley) 5/12/90
# $FreeBSD: src/usr.bin/yacc/Makefile,v 1.13.40.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	yacc
SRCS=	closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	skeleton.c symtab.c verbose.c warshall.c
SCRIPTS=yyfix.sh
MAN=	yacc.1 yyfix.1
LINKS=	${BINDIR}/yacc ${BINDIR}/byacc
MLINKS=	yacc.1 byacc.1

.include <bsd.prog.mk>
