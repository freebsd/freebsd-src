# $Id: Makefile,v 1.2 2000/07/14 18:16:29 iwasaki Exp $
# $FreeBSD$

PROG=	acpidump
MAN=	acpidump.8
SRCS=	acpi.c acpi_user.c asl_dump.c aml_dump.c acpidump.c
SRCS+=	aml_parse.c aml_name.c aml_amlmem.c aml_memman.c aml_store.c \
	aml_obj.c aml_evalobj.c aml_common.c

CFLAGS+= -I${.CURDIR}/../amldb

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../amldb/aml
