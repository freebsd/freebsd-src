PROG=	neqn
SRCS=	diacrit.c \
	e.y \
	y.tab.h \
	eqnbox.c \
	font.c \
	fromto.c \
	funny.c \
	glob.c \
	integral.c \
	io.c \
	lex.c \
	lookup.c \
	mark.c \
	matrix.c \
	move.c \
	over.c \
	paren.c \
	pile.c \
	shift.c \
	size.c \
	sqrt.c \
	text.c

MAN=

CLEANFILES+=	y.tab.h

CFLAGS+=	-I. -DNEON

.include <bsd.prog.mk>

.PATH: ${DOCTOOLSDIR}/eqn/neqn.d
