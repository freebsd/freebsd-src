LIBIMAGEBOX?=		${DESTDIR}${LIBDIR}/libimagebox.a
DPADD_imagebox=		${LIBIMAGEBOX} ${LIBVULN_PNG} ${LIBZ} ${LIBPTHREAD}
LDADD_imagebox=		-limagebox -lvuln_png -lz -lpthread

# Depends on png, but don't include to all png or vuln_png to be used.
LIBTERASIC_MTL?=	${DESTDIR}${LIBDIR}/libterasic_mtl.a
DPADD_terasic_mtl=	${LIBTERASIC_MTL}
LDADD_terasic_mtl=	-lterasic_mtl

LIBVULN_MAGIC?=		${DESTDIR}${LIBDIR}/libvuln_magic.a
DPADD_vuln_magic=	${LIBVULN_MAGIC} ${LIBZ}
LDADD_vuln_magic=	-lvuln_magic -lz

LIBVULN_PNG?=		${DESTDIR}${LIBDIR}/libvuln_png.a
DPADD_vuln_png=		${LIBVULN_PNG} ${LIBZ}
LDADD_vuln_png=		-lvuln_png -lz
