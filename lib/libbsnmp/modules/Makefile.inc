# $FreeBSD: src/lib/libbsnmp/modules/Makefile.inc,v 1.1 2003/11/10 09:17:33 harti Exp $

NOINSTALLLIB=	yes
NOPROFILE=	yes

LIB=	snmp_${MOD}
SHLIB_NAME= snmp_${MOD}.so.${SHLIB_MAJOR}

SRCS:=	${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h ${SRCS}

MIBSDIR= ${SHAREDIR}/bsnmp/mibs
DEFSDIR= ${SHAREDIR}/bsnmp/defs

FILESGROUPS += DEFS MIBS

.include "../Makefile.inc"
