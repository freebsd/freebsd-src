PROG=	grap
SRCS=	coord.c \
	for.c \
	frame.c \
	grap.y \
	grapl.c \
	input.c \
	label.c \
	main.c \
	misc.c \
	plot.c \
	print.c \
	ticks.c \
	version.c \
	y.tab.h

FILES=	grap.defines
FILESDIR=	${LIBDIR}

CLEANFILES=	grapl.c

CFLAGS=	-I. -I${DOCTOOLSDIR}/grap -DLIBDIR='"${LIBDIR}"'
LIBADD=	m

.include <bsd.prog.mk>

.PATH:	${DOCTOOLSDIR}/grap
