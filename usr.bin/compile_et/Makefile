#	$Id: Makefile,v 1.3 1995/01/14 22:29:29 wollman Exp $

PROG=	compile_et
SRCS=	compile_et.c error_table.y
CFLAGS+=	-I. -I${.CURDIR}/../../lib/libcom_err
CLEANFILES+=	et_lex.lex.c
DPADD+=	${LIBL}
LDADD+=	-ll

beforedepend:	et_lex.lex.c
error_table.o:	et_lex.lex.c

.l.c:
	${LEX} -l -t ${.IMPSRC} > ${.TARGET}

.include <bsd.prog.mk>

