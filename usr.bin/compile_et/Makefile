#	$Id: Makefile,v 1.7 1997/02/22 19:54:44 peter Exp $

PROG=	compile_et
SRCS=	compile_et.c error_table.c
CFLAGS+=-I. -I${.CURDIR}/../../lib/libcom_err
LFLAGS+=-l
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES=	error_table.c et_lex.lex.c y.tab.h

error_table.o: et_lex.lex.c

.include <bsd.prog.mk>

${DEPENDFILE}: et_lex.lex.c
