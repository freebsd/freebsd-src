
PROGS=	eqn checkeq
SRCS.eqn=	diacrit.c \
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

SRCS.checkeq=	checkeq.c

MAN=	eqn.1 \
	eqnchar.7
MLINKS=	eqn.1 checkeq.1 \
	eqn.1 neqn.1

CLEANFILES+=	y.tab.h

CFLAGS+=	-I.

.include <bsd.progs.mk>

.PATH: ${DOCTOOLSDIR}/eqn/eqn.d ${DOCTOOLSDIR}/eqn/checkeq.d
