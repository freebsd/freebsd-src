# $FreeBSD: src/sys/modules/bktr/bktr_mem/Makefile,v 1.4 2006/07/01 10:51:54 markm Exp $

.PATH: ${.CURDIR}/../../../dev/bktr

KMOD=	bktr_mem 
SRCS=	bktr_mem.c
EXPORT_SYMS=	bktr_has_stored_addresses	\
		bktr_store_address		\
		bktr_retrieve_address

.include <bsd.kmod.mk>
