#	@(#)Makefile	8.1 (Berkeley) 6/4/93

PROG=	getty
SRCS=	main.c init.c subr.c ttydefaults.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN5=	gettytab.5 ttys.5
MAN8=	getty.8

.include <bsd.prog.mk>
