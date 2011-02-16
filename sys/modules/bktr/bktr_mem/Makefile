# $FreeBSD: src/sys/modules/bktr/bktr_mem/Makefile,v 1.4.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/bktr

KMOD=	bktr_mem 
SRCS=	bktr_mem.c
EXPORT_SYMS=	bktr_has_stored_addresses	\
		bktr_store_address		\
		bktr_retrieve_address

.include <bsd.kmod.mk>
