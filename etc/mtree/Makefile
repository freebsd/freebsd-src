# $FreeBSD: src/etc/mtree/Makefile,v 1.1.8.1 2004/09/30 23:36:07 dougb Exp $

FILES=	${_BIND.chroot.dist} \
	${_BIND.include.dist} \
	BSD.include.dist \
	BSD.local.dist \
	BSD.root.dist \
	BSD.usr.dist \
	BSD.var.dist \
	BSD.x11-4.dist \
	BSD.x11.dist

.if !defined(NO_BIND)
_BIND.chroot.dist=	BIND.chroot.dist
.if defined(WITH_BIND_LIBS)
_BIND.include.dist=	BIND.include.dist
.endif
.endif

NOOBJ=	noobj
FILESDIR= /etc/mtree

.include <bsd.prog.mk>
