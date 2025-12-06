.include <src.opts.mk>

CONFGROUPS=	ETC ROOT
ETC=	profile
ROOT=	dot.shrc dot.profile
ROOTDIR=	/root
ROOTDIR_MODE=	0750
ROOTNAME_dot.shrc=	.shrc
ROOTNAME_dot.profile=	.profile
PACKAGE=runtime
PROG=	sh
INSTALLFLAGS= -S
SHSRCS=	alias.c arith_yacc.c arith_yylex.c cd.c echo.c error.c eval.c \
	exec.c expand.c \
	histedit.c input.c jobs.c kill.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c printf.c redir.c show.c \
	test.c trap.c var.c
GENSRCS= builtins.c nodes.c syntax.c
GENHDRS= builtins.h nodes.h syntax.h token.h
SRCS= ${SHSRCS} ${GENSRCS} ${GENHDRS}

# MLINKS for Shell built in commands for which there are no userland
# utilities of the same name are handled with the associated manpage,
# builtin.1 in share/man/man1/.

LIBADD=	edit

CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debug:
# DEBUG_FLAGS+= -g -DDEBUG=2 -fno-inline

.PATH:	${.CURDIR}/bltin \
	${.CURDIR:H}/kill \
	${.CURDIR:H}/test \
	${SRCTOP}/usr.bin/printf

CLEANFILES+= mknodes mksyntax
CLEANFILES+= ${GENSRCS} ${GENHDRS}

.if ${MACHINE} == "host" || ${MK_DIRDEPS_BUILD} == "no"
build-tools: mknodes mksyntax

DEPENDOBJS+= mknodes mksyntax
mknodes mksyntax: ${BUILD_TOOLS_META}
builtins.c builtins.h: mkbuiltins
syntax.c syntax.h: mksyntax
nodes.c nodes.h: mknodes
.endif

.ORDER: builtins.c builtins.h
builtins.h: .NOMETA
builtins.c builtins.h: builtins.def
	sh ${.CURDIR}/mkbuiltins ${.CURDIR}


.ORDER: nodes.c nodes.h
nodes.h: .NOMETA
nodes.c nodes.h: nodetypes nodes.c.pat
	${BTOOLSPATH:U.}/mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

.ORDER: syntax.c syntax.h
syntax.h: .NOMETA
syntax.c syntax.h:
	${BTOOLSPATH:U.}/mksyntax

token.h: mktokens
	sh ${.CURDIR}/mktokens

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
