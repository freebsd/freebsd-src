#	@(#)Makefile	8.1 (Berkeley) 6/19/93
#	$Id$

PROG=	routed
SRCS=	af.c if.c input.c main.c output.c startup.c tables.c timer.c \
	trace.c inet.c
MAN8=	routed.8
#SUBDIR=	query trace
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat

.include <bsd.prog.mk>
