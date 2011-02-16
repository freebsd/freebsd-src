# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.10.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_dtrace} \
	${_dtruss} \
	${_lockstat} \
	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_zdb=	zdb
.endif
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_dtrace=	dtrace
_dtruss=	dtruss
_lockstat=	lockstat
.endif

.include <bsd.subdir.mk>
