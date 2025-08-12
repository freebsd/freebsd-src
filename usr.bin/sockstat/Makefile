.include <src.opts.mk>

PROG=		sockstat
SRCS=		main.c sockstat.c

LIBADD=		jail xo

.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_net
LIBADD+=	cap_netdb
LIBADD+=	cap_pwd
LIBADD+=	cap_sysctl
CFLAGS+=	-DWITH_CASPER
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
