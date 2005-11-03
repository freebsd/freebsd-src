# $FreeBSD: src/etc/mtree/Makefile,v 1.5 2004/12/21 08:46:50 ru Exp $

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

NO_OBJ=
FILESDIR= /etc/mtree

.include <bsd.prog.mk>
