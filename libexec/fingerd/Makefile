
.include <src.opts.mk>

PROG=	fingerd
LIBADD=	util
MAN=	fingerd.8

WARNS?=	2
WFORMAT=0

.if ${MK_BLACKLIST_SUPPORT} != "no"
CFLAGS+= -DUSE_BLACKLIST -I${SRCTOP}/contrib/blocklist/include
LIBADD+= blacklist
LDFLAGS+=-L${LIBBLACKLISTDIR}
.endif

.include <bsd.prog.mk>
