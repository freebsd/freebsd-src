# $FreeBSD$

NOINSTALLLIB=	yes
NOPROFILE=	yes

LIB=	snmp_${MOD}
SHLIB_NAME= snmp_${MOD}.so.${SHLIB_MAJOR}

SRCS:=	${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h ${SRCS}

MIBSDIR= ${SHAREDIR}/bsnmp/mibs
DEFSDIR= ${SHAREDIR}/bsnmp/defs

FILESGROUPS += DEFS MIBS

.include "../Makefile.inc"
