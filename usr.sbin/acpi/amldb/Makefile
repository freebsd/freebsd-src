# $Id: Makefile,v 1.5 2000/07/14 18:16:30 iwasaki Exp $
# $FreeBSD$

PROG=	amldb
SRCS=	amldb.c debug.c region.c
SRCS+=	aml_parse.c aml_name.c aml_amlmem.c aml_memman.c aml_store.c aml_obj.c aml_evalobj.c aml_common.c
MAN8=	amldb.8
#DEBUG_FLAGS= -g
CFLAGS+= -I${.CURDIR}

.include <bsd.prog.mk>
.PATH: ${.CURDIR}/aml
