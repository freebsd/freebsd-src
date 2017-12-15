# $FreeBSD$

.include <bsd.init.mk>

SUBDIR=		boot1.chrp ofw uboot
.if ${MK_FDT} == "yes"
SUBDIR+=	kboot
.endif

.include <bsd.subdir.mk>
