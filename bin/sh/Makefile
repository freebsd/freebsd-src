#	@(#)Makefile	8.4 (Berkeley) 5/5/95
#	$FreeBSD$

PROG=	sh
SHSRCS=	alias.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c parser.c printf.c redir.c show.c \
	trap.c output.c var.c

GENSRCS=arith.c arith_lex.c builtins.c init.c nodes.c syntax.c

SRCS=	${GENSRCS} ${SHSRCS}

DPADD+= ${LIBL} ${LIBEDIT} ${LIBTERMCAP}
LDADD+= -ll -ledit -ltermcap

LFLAGS= -8	# 8-bit lex scanner for arithmetic
CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debug:
# CFLAGS+= -g -DDEBUG=2

.PATH:	${.CURDIR}/bltin ${.CURDIR}/../../usr.bin/printf

CLEANFILES+=	builtins.h mkinit mkinit.o mknodes mknodes.o \
		mksyntax mksyntax.o \
		nodes.h syntax.h token.h y.tab.h
CLEANFILES+=	${GENSRCS}

beforedepend: token.h

builtins.h builtins.c: mkbuiltins builtins.def
	cd ${.CURDIR}; sh mkbuiltins ${.OBJDIR}

init.c: mkinit ${SHSRCS}
	./mkinit ${.ALLSRC:S/^mkinit$//}

# XXX this is just to stop the default .c rule being used, so that the
# intermediate object has a fixed name.
# XXX we have a default .c rule, but no default .o rule.
.o:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
mkinit: mkinit.o
mkinit.o: mkinit.c		# XXX and many headers
mknodes: mknodes.o
mknodes.o: mknodes.c		# XXX and many headers
mksyntax: mksyntax.o
mksyntax.o: mksyntax.c		# XXX and many headers

nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

parser.o: token.h

syntax.c syntax.h: mksyntax
	./mksyntax

token.h: mktokens
	sh ${.CURDIR}/mktokens

.include <bsd.prog.mk>
