PROG=	gctags
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../lib -DGLOBAL -DYACC
LDADD=  $(LIBUTIL)
DPADD=  $(LIBUTIL)
SRCS=	C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c assembler.c

.include <bsd.prog.mk>
