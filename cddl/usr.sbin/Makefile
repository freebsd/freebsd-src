# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.7.2.2.4.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	dtrace \
	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
