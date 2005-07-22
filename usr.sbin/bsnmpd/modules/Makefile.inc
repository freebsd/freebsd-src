# $FreeBSD$

SHLIB_MAJOR=	4
WARNS?=		6
INCSDIR=	${INCLUDEDIR}/bsnmp

SHLIB_NAME=	snmp_${MOD}.so.${SHLIB_MAJOR}
SRCS+=		${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CLEANFILES+=	${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CFLAGS+=	-I${CONTRIB}/lib -I${CONTRIB}/snmpd -I.

${MOD}_oid.h: ${MOD}_tree.def ${EXTRAMIBDEFS}
	cat ${.ALLSRC} | gensnmptree -e ${XSYM} > ${.TARGET}

.ORDER: ${MOD}_tree.c ${MOD}_tree.h
${MOD}_tree.c ${MOD}_tree.h: ${MOD}_tree.def ${EXTRAMIBDEFS}
	cat ${.ALLSRC} | gensnmptree -l -p ${MOD}_

.if defined(DEFS)
FILESGROUPS+=	DEFS
.endif
DEFSDIR=	${SHAREDIR}/snmp/defs

.if defined(BMIBS)
FILESGROUPS+=	BMIBS
.endif
BMIBSDIR=	${SHAREDIR}/snmp/mibs

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
