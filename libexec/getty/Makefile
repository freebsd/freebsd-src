#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/getty/Makefile,v 1.9 1999/08/28 00:09:34 peter Exp $

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN5=	gettytab.5 ttys.5
MAN8=	getty.8
# for the paranoid:
#CFLAGS+= -Wall -Wstrict-prototypes -Wno-unused -Wwrite-strings

.include <bsd.prog.mk>

