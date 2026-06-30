.include <src.opts.mk>

PROG=	rev

.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_fileargs
CFLAGS+=	-DWITH_CASPER
.endif

.include <bsd.prog.mk>
