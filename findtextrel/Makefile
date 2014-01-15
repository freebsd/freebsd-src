# $Id: Makefile 2069 2011-10-26 15:53:48Z jkoshy $

TOP=	..

PROG=	findtextrel
SRCS=	findtextrel.c

WARNS?=	6

DPADD=	${LIBELFTC} ${LIBDWARF} ${LIBELF} 
LDADD=	-lelftc -ldwarf -lelf

MAN1=	findtextrel.1

.include "${TOP}/mk/elftoolchain.prog.mk"
