PROG=	awk
SRCS=	main.c eval.c builtin.c msg.c iop.c io.c field.c getopt1.c \
	getopt.c array.c \
	node.c version.c re.c awk.c dfa.c
CFLAGS+=-I${.CURDIR} -DGAWK
CLEANFILES+=awk.c y.tab.h

SUBDIR+= doc

DPADD+= ${LIBGNUREGEX} ${LIBM}
LDADD+= -lgnuregex -lm

.include <bsd.prog.mk>
