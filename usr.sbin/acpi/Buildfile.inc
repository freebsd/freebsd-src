# $FreeBSD$

ACPICA_DIR = ${BSDSRCTOP}/sys/contrib/dev/acpica

.PATH:	${ACPICA_DIR} ${ACPICA_DIR}/compiler ${ACPICA_DIR}/common

CFLAGS += -I${BSDSRCTOP}/sys

.if exists(${.CURDIR}/../../Buildfile.inc)
.include "${.CURDIR}/../../Buildfile.inc"
.endif
