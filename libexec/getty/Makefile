#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/getty/Makefile,v 1.9.2.1 2001/04/25 10:40:52 ru Exp $

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	gettytab.5 ttys.5 getty.8
# for the paranoid:
#CFLAGS+= -Wall -Wstrict-prototypes -Wno-unused -Wwrite-strings

.include <bsd.prog.mk>

