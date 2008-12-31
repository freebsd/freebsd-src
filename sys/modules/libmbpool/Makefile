# $FreeBSD: src/sys/modules/libmbpool/Makefile,v 1.1.28.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	libmbpool
SRCS=	subr_mbpool.c

EXPORT_SYMS=	mbp_create	\
		mbp_destroy	\
		mbp_alloc	\
		mbp_free	\
		mbp_ext_free	\
		mbp_card_free	\
		mbp_count	\
		mbp_get		\
		mbp_get_keep	\
		mbp_sync

.include <bsd.kmod.mk>
