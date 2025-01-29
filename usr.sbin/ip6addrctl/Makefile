.include <src.opts.mk>

PACKAGE=	runtime
PROG=	ip6addrctl
MAN=	ip6addrctl.8

.if ${MK_JAIL} != "no"
CFLAGS+= -DJAIL
LIBADD+= jail
.endif

.include <bsd.prog.mk>
