.include <src.opts.mk>

PROG=	lsvfs

.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_sysctl
CFLAGS+=	-DWITH_CASPER
.endif

.include <bsd.prog.mk>
