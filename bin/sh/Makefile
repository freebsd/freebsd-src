#	Makefile,v 1.7 1993/08/09 04:58:18 mycroft Exp

PROG=	sh
SRCS=	builtins.c cd.c dirent.c echo.c error.c eval.c exec.c expand.c \
	input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c nodes.c options.c parser.c redir.c show.c \
	syntax.c trap.c output.c var.c
OBJS+=	init.o
CFLAGS+=-DSHELL -I. -I${.CURDIR}
.PATH:	${.CURDIR}/bltin
CLEANFILES+=\
	builtins.c builtins.h init.c mkinit mknodes mksyntax \
	nodes.c nodes.h syntax.c syntax.h token.def

.depend parser.o: token.def
token.def: mktokens
	sh ${.CURDIR}/mktokens

builtins.h builtins.c: ${.CURDIR}/mkbuiltins ${.CURDIR}/builtins
	cd ${.CURDIR}; sh mkbuiltins

init.c: mkinit ${SRCS}
	./mkinit '${CC} -c ${CFLAGS} init.c' ${.ALLSRC}
	touch ${.TARGET}

mkinit: ${.CURDIR}/mkinit.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.CURDIR}/mkinit.c -o $@ ${LDADD}

nodes.c nodes.h: mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

mknodes: ${.CURDIR}/mknodes.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.CURDIR}/mknodes.c -o $@ ${LDADD}

syntax.c syntax.h: mksyntax
	./mksyntax

mksyntax: ${.CURDIR}/mksyntax.c ${.CURDIR}/parser.h
	${CC} ${CFLAGS} ${LDFLAGS} ${.CURDIR}/mksyntax.c -o $@ ${LDADD}

.include <bsd.prog.mk>
