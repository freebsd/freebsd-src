# $FreeBSD$

MAINTAINER=     mbr

WARNS?=		0
CFLAGS+= -I${DIST_DIR}/includes -DENABLE_POLLING_MODE -DNOMINUM

CLIENTBINDIR=	/sbin
RUNDIR=		/var/run
ETCDIR=		/etc
DBDIR=		/var/db

MANFILTER= sed	-e 's|DBDIR|${DBDIR}|g' \
		-e 's|ETCDIR|${ETCDIR}|g' \
		-e 's|CLIENTBINDIR|${CLIENTBINDIR}|g' \
		-e 's|RUNDIR|${RUNDIR}|g'

.if exists(${.OBJDIR}/../common)
LIBDHCP=	${.OBJDIR}/../common/libdhcp.a
.else
LIBDHCP=	${.CURDIR}/../common/libdhcp.a
.endif

.if exists(${.OBJDIR}/../minires)
LIBRES=		${.OBJDIR}/../minires/libres.a
.else
LIBRES=		${.CURDIR}/../minires/libres.a
.endif

.if exists(${.OBJDIR}/../omapip)
LIBOMAPI=	${.OBJDIR}/../omapip/libomapi.a
.else
LIBOMAPI=	${.CURDIR}/../omapip/libomapi.a
.endif

.if exists(${.OBJDIR}/../dst)
LIBDST= 	${.OBJDIR}/../dst/libdst.a
.else
LIBDST=		${.CURDIR}/../dst/libdst.a
.endif

.if exists(${.OBJDIR}/../dhcpctl)
LIBDHCPCTL= 	${.OBJDIR}/../dhcpctl/libdhcpctl.a
.else
LIBDHCPCTL=	${.CURDIR}/../dhcpctl/libdhcpctl.a
.endif

.include "../Makefile.inc"
