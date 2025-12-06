.include <src.opts.mk>

PROG=	fingerd
LIBADD=	util
MAN=	fingerd.8

WARNS?=	2
WFORMAT=0

.if ${MK_BLOCKLIST_SUPPORT} != "no"
CFLAGS+= -DUSE_BLOCKLIST -I${SRCTOP}/contrib/blocklist/include
LIBADD+= blocklist
LDFLAGS+=-L${LIBBLOCKLISTDIR}
.endif

.include <bsd.prog.mk>
