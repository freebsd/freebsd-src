#	@(#)Makefile	8.1 (Berkeley) 6/4/93

PROG=	getty
SRCS=	main.c init.c subr.c ttydefaults.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN5=	gettytab.0 ttys.0
MAN8=	getty.0

.include <bsd.prog.mk>
