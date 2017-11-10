# $FreeBSD$

.include <bsd.init.mk>

SUBDIR=		boot1.chrp ofw uboot
.if ${MACHINE_ARCH} != "powerpcspe"
SUBDIR+=	ps3
.endif
.if ${MK_FDT} == "yes"
SUBDIR+=	kboot
.endif

.include <bsd.subdir.mk>
