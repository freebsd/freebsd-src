#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	mkastosc
SRCS=	mkastosc.c dohits.c asc_ebc.c ebc_disp.c
CFLAGS+=-I${.CURDIR}/../mkhits -I${.CURDIR}/.. -I.
NOMAN=	noman
.PATH:	${.CURDIR}/../mkhits ${.CURDIR}/../../api

.include <bsd.prog.mk>
