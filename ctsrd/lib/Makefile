# $FreeBSD$

.include <src.opts.mk>

SUBDIR_ORDERED=	libvuln_png

SUBDIR=	${SUBDIR_ORDERED} \
	libterasic_mtl \
	libvuln_magic

# XXX: libimagebox should be buildable without CHERI
.if ${MK_CHERI} != "no"
SUBDIR_ORDERED+=	libvuln_png_cheri
SUBDIR+=	libcompositor \
		libimagebox
.endif

.include <bsd.subdir.mk>
