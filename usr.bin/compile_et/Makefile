#	$Id: Makefile,v 1.9 1998/05/04 18:40:18 bde Exp $

PROG=	compile_et
SRCS=	compile_et.c error_table.y
CFLAGS+=-I. -I${.CURDIR}/../../lib/libcom_err
LFLAGS+=-l
YFLAGS=
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES=	et_lex.lex.c

error_table.o: et_lex.lex.c

.include <bsd.prog.mk>

${DEPENDFILE}: et_lex.lex.c
