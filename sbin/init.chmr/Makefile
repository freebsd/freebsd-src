PROG=	init
MAN8=   init.8
SRCS=	init.c ttytab.c configure.c cf_table.c utils.c fake_syslog.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
LDFLAGS=

.include <bsd.prog.mk>
