.include <src.opts.mk>

PROG=	paste

.if ${MK_CASPER} != "no" && !defined(RESCUE)
LIBADD+= casper
LIBADD+= cap_fileargs
CFLAGS+= -DWITH_CASPER
.endif

.include <bsd.prog.mk>
