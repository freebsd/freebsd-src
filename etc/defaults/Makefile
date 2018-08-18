# $FreeBSD$

.include <src.opts.mk>

FILES=	devfs.rules
FILESDIR= /etc/defaults

.if ${MK_BLUETOOTH} != "no"
FILES+=	bluetooth.device.conf
.endif

.include <bsd.prog.mk>
