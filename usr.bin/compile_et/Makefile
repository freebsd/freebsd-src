#	$Id: Makefile,v 1.5 1995/02/09 00:24:00 jkh Exp $

PROG=	compile_et
SRCS=	compile_et.c error_table.y
SRCS+=	y.tab.h
CFLAGS+=	-I. -I${.CURDIR}/../../lib/libcom_err
CLEANFILES+=	et_lex.lex.c y.tab.h
DPADD+=	${LIBL}
LDADD+=	-ll

beforedepend:	et_lex.lex.c
error_table.o:	et_lex.lex.c

.l.c:
	${LEX} -l -t ${.IMPSRC} > ${.TARGET}

.include <bsd.prog.mk>

