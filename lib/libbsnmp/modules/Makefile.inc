# $FreeBSD$

SHLIB_NAME=	snmp_${MOD}.so.${SHLIB_MAJOR}
SRCS+=		${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CLEANFILES+=	${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CFLAGS+=	-I${CONTRIB}/lib -I${CONTRIB}/snmpd -I.

${MOD}_oid.h: ${MOD}_tree.def
	gensnmptree -e ${XSYM} < ${.ALLSRC} > ${.TARGET}

.ORDER: ${MOD}_tree.c ${MOD}_tree.h
${MOD}_tree.c ${MOD}_tree.h: ${MOD}_tree.def
	gensnmptree -l -p ${MOD}_ < ${.ALLSRC}

.if defined(DEFS)
FILESGROUPS+=	DEFS
DEFSDIR=	${SHAREDIR}/bsnmp/defs
.endif

.if defined(MIBS)
FILESGROUPS+=	MIBS
MIBSDIR=	${SHAREDIR}/bsnmp/mibs
.endif

.include "../Makefile.inc"
