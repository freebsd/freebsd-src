#	From: @(#)Makefile	5.14 (Berkeley) 2/26/91
#	$Id: Makefile,v 1.1 1993/09/07 14:13:06 rgrimes Exp $

PROG=	XNSrouted
MAN8=	XNSrouted.8
SRCS=	af.c if.c input.c main.c output.c startup.c tables.c timer.c trace.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
