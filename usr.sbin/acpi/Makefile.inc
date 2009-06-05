# $Id: Makefile.inc,v 1.1 2000/07/14 18:16:22 iwasaki Exp $
# $FreeBSD$

ACPICA_DIR= ${.CURDIR}/../../../sys/contrib/dev/acpica
CFLAGS+= -I${.CURDIR}/../../../sys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.PATH:	${ACPICA_DIR}			\
	${ACPICA_DIR}/common		\
	${ACPICA_DIR}/compiler		\
	${ACPICA_DIR}/debugger		\
	${ACPICA_DIR}/disassembler	\
	${ACPICA_DIR}/dispatcher	\
	${ACPICA_DIR}/events		\
	${ACPICA_DIR}/executer		\
	${ACPICA_DIR}/hardware		\
	${ACPICA_DIR}/namespace		\
	${ACPICA_DIR}/parser		\
	${ACPICA_DIR}/resources		\
	${ACPICA_DIR}/tables		\
	${ACPICA_DIR}/utilities
