# $FreeBSD: src/etc/mtree/Makefile,v 1.6.12.1 2010/02/10 00:26:20 kensmith Exp $

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
