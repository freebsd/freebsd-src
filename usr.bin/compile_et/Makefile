#	$FreeBSD$

PROG=	compile_et
SRCS=	compile_et.c error_table.y
CFLAGS+=	-I. -I${.CURDIR}/../../lib/libcom_err
CLEANFILES+=	et_lex.lex.c y.tab.h
DPADD+=	${LIBL}
LDADD+=	-ll

beforedepend:	et_lex.lex.c
error_table.o:	et_lex.lex.c

.l.c:
	${LEX} -l -t ${.IMPSRC} > ${.TARGET}

.include <bsd.prog.mk>

