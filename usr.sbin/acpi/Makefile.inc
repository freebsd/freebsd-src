# $Id: Makefile.inc,v 1.1 2000/07/14 18:16:22 iwasaki Exp $
# $FreeBSD: src/usr.sbin/acpi/Makefile.inc,v 1.9.6.1 2008/11/25 02:59:29 kensmith Exp $

ACPICA_DIR= ${.CURDIR}/../../../sys/contrib/dev/acpica
CFLAGS+= -I${.CURDIR}/../../../sys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.PATH:	${ACPICA_DIR} ${ACPICA_DIR}/compiler ${ACPICA_DIR}/common
