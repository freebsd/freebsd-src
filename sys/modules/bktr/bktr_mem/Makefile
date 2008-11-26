# $FreeBSD: src/sys/modules/bktr/bktr_mem/Makefile,v 1.3.14.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/bktr

KMOD=	bktr_mem 
SRCS=	bktr_mem.c
EXPORT_SYMS=	bktr_has_stored_addresses	\
		bktr_store_address		\
		bktr_retrieve_address

.include <bsd.kmod.mk>
