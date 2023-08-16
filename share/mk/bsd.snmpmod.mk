
INCSDIR=	${INCLUDEDIR}/bsnmp

SHLIB_NAME=	snmp_${MOD}.so.${SHLIB_MAJOR}
SRCS+=		${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CLEANFILES+=	${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CFLAGS+=	-I.
GENSNMPTREEFLAGS+=	-I${SHAREDIR}/snmpdefs


${MOD}_oid.h: ${MOD}_tree.def ${EXTRAMIBDEFS} ${EXTRAMIBSYMS}
	cat ${.ALLSRC} | gensnmptree ${GENSNMPTREEFLAGS} -e ${XSYM} > ${.TARGET}

# Multi-output targets both expect a .meta file and will fight over it. Only
# allow it on the .c file instead.
${MOD}_tree.h: ${MOD}_tree.c .NOMETA
# Force rebuild the .c file if any of its other outputs are missing.
.if !exists(${MOD}_tree.h)
${MOD}_tree.c: .PHONY .META
.endif
${MOD}_tree.c: ${MOD}_tree.def ${EXTRAMIBDEFS}
	cat ${.ALLSRC} | gensnmptree -f ${GENSNMPTREEFLAGS} -p ${MOD}_

.if defined(DEFS)
FILESGROUPS+=	DEFS
DEFSDIR?=	${SHAREDIR}/snmp/defs
.endif

.if defined(BMIBS)
FILESGROUPS+=	BMIBS
BMIBSDIR?=	${SHAREDIR}/snmp/mibs
.endif

DEFSPACKAGE=	bsnmp
BMIBSPACKAGE=	bsnmp

.if !target(smilint) && !empty(BMIBS)
LOCALBASE?=	/usr/local

SMILINT?=	${LOCALBASE}/bin/smilint

SMIPATH?=	${BMIBSDIR}:${LOCALBASE}/share/snmp/mibs

SMILINT_FLAGS?=	-c /dev/null -l6 -i group-membership

smilint: ${BMIBS}
	SMIPATH=${SMIPATH} ${SMILINT} ${SMILINT_FLAGS} ${.ALLSRC}
.endif
smilint: .PHONY

.include <bsd.lib.mk>
