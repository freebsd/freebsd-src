# Makefile for less
#
# $FreeBSD$

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c decode.c \
	edit.c filename.c forwback.c help.c ifile.c input.c jump.c line.c \
	linenum.c lsystem.c mark.c optfunc.c option.c opttbl.c os.c output.c \
	position.c prompt.c search.c signal.c tags.c ttyin.c version.c
DPADD=	${LIBNCURSES}
LDADD=	-lncurses

LINKS=	${BINDIR}/less /usr/bin/more

MAN1=	less.1
MANDEPEND=	less.1
CLEANFILES+=	${MANDEPEND}

MLINKS= less.1 more.1

.include "Makefile.common"
.include <bsd.prog.mk>
