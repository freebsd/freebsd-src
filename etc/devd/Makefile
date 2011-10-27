# $FreeBSD$

FILES=	uath.conf usb.conf

.if ${MACHINE} == "powerpc"
FILES+=	apple.conf
.endif

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
FILES+=	asus.conf
.endif

NO_OBJ=
FILESDIR=	/etc/devd
FILESMODE=	644

.include <bsd.prog.mk>
