# $FreeBSD$

PROG=	cached
MAN=	cached.conf.5 cached.8

WARNS?=	2
SRCS=	agent.c cached.c cachedcli.c cachelib.c cacheplcs.c debug.c log.c \
	config.c query.c mp_ws_query.c mp_rs_query.c singletons.c protocol.c \
	parser.c
CFLAGS+= -DCONFIG_PATH="\"${PREFIX}/etc/cached.conf\""
DPADD=	${LIBM} ${LIBPTHREAD} ${LIBUTIL}
LDADD=	-lm -lpthread -lutil

.PATH: ${.CURDIR}/agents
.include "${.CURDIR}/agents/Makefile.inc"
.include <bsd.prog.mk>
