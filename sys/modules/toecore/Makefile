
.PATH: ${SRCTOP}/sys/netinet

KMOD=	toecore
SRCS=	toecore.c
SRCS+=	opt_ofed.h opt_inet.h opt_inet6.h opt_kern_tls.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
