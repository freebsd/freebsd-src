#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	gctags
CFLAGS+=-I${.CURDIR} -DGTAGS -DBUGFIX -DMODIFY -DYACC
SRCS=	C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c assembler.c

.include <bsd.prog.mk>
