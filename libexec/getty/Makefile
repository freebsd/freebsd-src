#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.5 1996/05/05 19:01:06 joerg Exp $

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN5=	gettytab.5 ttys.5
MAN8=	getty.8
# for the paranoid:
#CFLAGS+= -Wall -Wstrict-prototypes -Wno-unused -Wwrite-strings

.include <bsd.prog.mk>
