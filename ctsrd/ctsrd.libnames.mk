LOCAL_LIBRARIES+=	imagebox terasic_mtl vuln_magic vuln_png

LIBIMAGEBOXDIR=		ctsrd/lib/libimagebox
LIBIMAGEBOX?=		${DESTDIR}${LIBDIR}/libimagebox.a
_DP_imagebox=		vuln_png pthread

# Depends on png, but don't include to all png or vuln_png to be used.
LIBTERASIC_MTLDIR=	ctsrd/lib/libterasic_mtl
LIBTERASIC_MTL?=	${DESTDIR}${LIBDIR}/libterasic_mtl.a
_DP_terasic_mtl=	png

LIBVULN_MAGICDIR=	ctsrd/lib/libvuln_magic
LIBVULN_MAGIC?=		${DESTDIR}${LIBDIR}/libvuln_magic.a
_DP_vuln_magic=		z

LIBVULN_PNGDIR=		ctsrd/lib/libvuln_png
LIBVULN_PNG?=		${DESTDIR}${LIBDIR}/libvuln_png.a
_DP_vuln_png=		z
