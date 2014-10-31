# $FreeBSD$

.include <src.opts.mk>

.if ${MACHINE} == "powerpc"
FILES+=	apple.conf
.endif

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
FILES+=	asus.conf
.if ${MK_HYPERV} != "no"
FILES+=	hyperv.conf
.endif
.endif

.if ${MK_USB} != "no"
FILES+=	uath.conf ulpt.conf usb.conf
.endif

.if ${MK_ZFS} != "no"
FILES+=	zfs.conf
.endif

NO_OBJ=
FILESDIR=	/etc/devd
FILESMODE=	644

.include <bsd.prog.mk>
