# $FreeBSD: src/etc/mtree/Makefile,v 1.6.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

FILES=	${_BIND.chroot.dist} \
	${_BIND.include.dist} \
	BSD.include.dist \
	BSD.local.dist \
	BSD.root.dist \
	BSD.usr.dist \
	BSD.var.dist \
	BSD.x11-4.dist \
	BSD.x11.dist

.if ${MK_BIND} != "no"
_BIND.chroot.dist=	BIND.chroot.dist
.if ${MK_BIND_LIBS} != "no"
_BIND.include.dist=	BIND.include.dist
.endif
.endif

NO_OBJ=
FILESDIR= /etc/mtree

.include <bsd.prog.mk>
