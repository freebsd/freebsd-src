# $FreeBSD: src/gnu/usr.bin/binutils/Makefile,v 1.25 2003/06/14 19:32:51 obrien Exp $

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as gasp ld nm objcopy objdump ranlib readelf \
		size strings strip doc

TARGET_ARCH?=	${MACHINE_ARCH}
.if (${TARGET_ARCH} == "alpha" \
  || ${TARGET_ARCH} == "i386" ) \
    && !defined(NO_GDB)
SUBDIR+=	gdb gdbreplay
.endif

.include <bsd.subdir.mk>
