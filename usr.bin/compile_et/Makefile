#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.2 1994/07/19 19:21:23 g89r4222 Exp $

PROG=	compile_et
CFLAGS+=-I. -I${.CURDIR}
SRCS=	compile_et.c error_message.c et_name.c init_et.c perror.c
OBJS+=	error_table.o
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES=et_lex.lex.c y.tab.c y.tab.h error_table.c
NOMAN=	noman

error_table.c: et_lex.lex.c

.include <bsd.prog.mk>
