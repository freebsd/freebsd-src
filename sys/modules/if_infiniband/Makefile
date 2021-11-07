# $FreeBSD$

.PATH: ${SRCTOP}/sys/net

KMOD=	if_infiniband
SRCS=	if_infiniband.c \
	opt_inet.h \
	opt_inet6.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
