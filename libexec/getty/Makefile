#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.7 1994/12/22 10:27:06 cgd Exp $

PROG=	getty
SRCS=	main.c init.c subr.c
DPADD+=	${LIBUTIL}
LDADD+=	-lutil
MAN=	getty.8 gettytab.5 ttys.5

.include <bsd.prog.mk>
