# $Id: Makefile.inc,v 1.1 2000/07/14 18:16:22 iwasaki Exp $
# $FreeBSD: src/usr.sbin/acpi/Makefile.inc,v 1.9.8.1 2009/04/15 03:14:26 kensmith Exp $

ACPICA_DIR= ${.CURDIR}/../../../sys/contrib/dev/acpica
CFLAGS+= -I${.CURDIR}/../../../sys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.PATH:	${ACPICA_DIR} ${ACPICA_DIR}/compiler ${ACPICA_DIR}/common
