#	@(#)Makefile	5.12 (Berkeley) 1/21/91

PROG=	getty
SRCS=   main.c init.c subr.c gettytab.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN5=	gettytab.5 ttys.5
MAN8=	getty.8

.include <bsd.prog.mk>
