#	$Id: Makefile,v 1.1 1997/03/16 07:08:10 gibbs Exp $

PROG=	aic7xxx_asm

CSRCS=	aic7xxx_asm.c symbol.c
GENSRCS=	gram.c scan.c

GENHDRS=	y.tab.h token.h
# Hack so non root users you can build kernels
BINOWN!= id -u -n
BINGRP!= id -g -n

SRCS=	${GENSRCS} ${CSRCS}
CLEANFILES+=	${GENSRCS} ${GENHDRS} y.output
DPADD+=	${LIBL}
LDADD+=	-ll

CFLAGS+= -I.
NOMAN=	noman
.include <bsd.prog.mk>
