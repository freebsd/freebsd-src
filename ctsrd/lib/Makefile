# $FreeBSD$

SUBDIR_ORDERED=	libvuln_png

SUBDIR=	${SUBDIR_ORDERED} \
	libterasic_mtl \
	libvuln_magic

# XXX: libimagebox should be buildable without CHERI
.if ${MK_CHERI} != "no"
SUBDIR+=	libcompositor \
		libimagebox
.endif

.include <bsd.subdir.mk>
