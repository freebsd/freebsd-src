
.include <src.opts.mk>

PROG=		sockstat

LIBADD=		jail

.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_net
LIBADD+=	cap_netdb
LIBADD+=	cap_pwd
LIBADD+=	cap_sysctl
CFLAGS+=	-DWITH_CASPER
.endif

.include <bsd.prog.mk>
