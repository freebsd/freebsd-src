# $FreeBSD$

MAINTAINER=     murray

WARNS=		0
CFLAGS+= -I${DIST_DIR}/includes

CLIENTBINDIR=	/sbin
RUNDIR=		/var/run
ETCDIR=		/etc
DBDIR=		/var/db

MANFILTER= perl -pe 's\#DBDIR\#${DBDIR}\#g;s\#ETCDIR\#${ETCDIR}\#g;s\#CLIENTBINDIR\#${CLIENTBINDIR}\#g;s\#RUNDIR\#${RUNDIR}\#g;'

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

.include "../Makefile.inc"
