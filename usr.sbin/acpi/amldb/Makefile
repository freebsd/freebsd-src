# $Id: Makefile,v 1.5 2000/07/14 18:16:30 iwasaki Exp $
# $FreeBSD: src/usr.sbin/acpi/amldb/Makefile,v 1.7 2001/10/22 17:25:32 iwasaki Exp $

PROG=	amldb
MAN=	amldb.8
SRCS=	amldb.c debug.c region.c
SRCS+=	aml_parse.c aml_name.c aml_amlmem.c aml_memman.c aml_store.c \
	aml_obj.c aml_evalobj.c aml_common.c

CFLAGS+= -I${.CURDIR}

.include <bsd.prog.mk>
.PATH: ${.CURDIR}/aml
