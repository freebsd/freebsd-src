# $FreeBSD: src/sys/modules/bktr/bktr_mem/Makefile,v 1.3 2002/01/11 01:15:58 msmith Exp $

MAINTAINER=	roger@freebsd.org

.PATH: ${.CURDIR}/../../../dev/bktr

KMOD=	bktr_mem 
SRCS=	bktr_mem.c
EXPORT_SYMS=	bktr_has_stored_addresses	\
		bktr_store_address		\
		bktr_retrieve_address

.include <bsd.kmod.mk>
