# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

.include <src.opts.mk>

CONTRIB=${.CURDIR}/../../../contrib/bsnmp
.PATH: ${CONTRIB}/snmpd

PROG=	bsnmpd
SRCS=	main.c action.c config.c export.c trap.c trans_udp.c trans_lsock.c
SRCS+=	oid.h tree.c tree.h
XSYM=	snmpMIB begemotSnmpdModuleTable begemotSnmpd begemotTrapSinkTable \
	sysUpTime snmpTrapOID coldStart authenticationFailure \
	begemotSnmpdTransUdp begemotSnmpdTransLsock begemotSnmpdLocalPortTable \
	freeBSD freeBSDVersion
CLEANFILES= oid.h tree.c tree.h
MAN=	bsnmpd.1 snmpmod.3

FILESGROUPS= BMIBS DEFS

BMIBS=	FOKUS-MIB.txt BEGEMOT-MIB.txt BEGEMOT-SNMPD.txt
BMIBSDIR= ${SHAREDIR}/snmp/mibs
DEFS=	tree.def
DEFSDIR= ${SHAREDIR}/snmp/defs

CFLAGS+= -DSNMPTREE_TYPES
CFLAGS+= -I${CONTRIB}/lib -I${CONTRIB}/snmpd -I. -DUSE_LIBBEGEMOT
CFLAGS+= -DUSE_TCPWRAPPERS
CFLAGS+= -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DHAVE_ERR_H -DHAVE_STRLCPY
LIBADD=	begemot bsnmp wrap

LDFLAGS= -Wl,-export-dynamic

.if ${MK_OPENSSL} != "no"
CFLAGS+= -DHAVE_LIBCRYPTO
.endif

oid.h: tree.def Makefile
	gensnmptree -e ${XSYM} < ${.ALLSRC:M*.def} > ${.TARGET}

.ORDER: tree.c tree.h
tree.h: .NOMETA
tree.c tree.h: tree.def
	gensnmptree -l < ${.ALLSRC}

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'

NO_WCAST_ALIGN=	yes
WARNS?=		6

.include <bsd.prog.mk>
