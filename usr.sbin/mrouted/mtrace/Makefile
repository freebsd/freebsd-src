#	$Id: Makefile,v 1.4 1996/06/24 04:25:22 jkh Exp $

PROG=	mtrace

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S

SRCS=	mtrace.c
MAN8=	${.CURDIR}/../mtrace.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
