PROG=	pic
SRCS=	arcgen.c \
	blockgen.c \
	boxgen.c \
	circgen.c \
	for.c \
	input.c \
	linegen.c \
	main.c \
	misc.c \
	movegen.c \
	picl.c \
	picy.y \
	pltroff.c \
	print.c \
	symtab.c \
	textgen.c \
	version.c \
	y.tab.h

LIBADD=	m

CFLAGS+=	-I${DOCTOOLSDIR}/pic -I.

.include <bsd.prog.mk>
.PATH: ${DOCTOOLSDIR}/pic
