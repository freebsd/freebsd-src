# $FreeBSD$

.include <src.opts.mk>

SUBDIR_ORDERED=	libvuln_png

SUBDIR=	${SUBDIR_ORDERED} \
	libterasic_mtl \
	libvuln_magic

# XXX: libimagebox should be buildable without CHERI
.if ${MK_CHERI} != "no"
SUBDIR+=	libimagebox
.endif

.if defined(LIBCHERI)
SUBDIR:=	libvuln_png
.endif

.include <bsd.subdir.mk>
