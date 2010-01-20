# $FreeBSD$

.include <bsd.own.mk>

FILES=	${_BIND.chroot.dist} \
	${_BIND.include.dist} \
	BSD.include.dist \
	BSD.root.dist \
	${_BSD.sendmail.dist} \
	BSD.usr.dist \
	BSD.var.dist

.if ${MK_BIND} != "no"
_BIND.chroot.dist=	BIND.chroot.dist
.if ${MK_BIND_LIBS} != "no"
_BIND.include.dist=	BIND.include.dist
.endif
.endif
.if ${MK_SENDMAIL} != "no"
_BSD.sendmail.dist=	BSD.sendmail.dist
.endif

NO_OBJ=
FILESDIR= /etc/mtree

.include <bsd.prog.mk>
