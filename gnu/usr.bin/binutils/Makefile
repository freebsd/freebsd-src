# $FreeBSD: src/gnu/usr.bin/binutils/Makefile,v 1.11.2.6 2004/06/29 03:32:11 imp Exp $

#	In my absence, bde and jdp have pre-approval for any commit they feel
#	necessary.
MAINTAINER=	obrien@FreeBSD.org

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as gasp ld nm objcopy objdump ranlib readelf \
		size strings strip doc

TARGET_ARCH?=	${MACHINE_ARCH}
.if (${TARGET_ARCH} == "alpha" || ${TARGET_ARCH} == "i386") \
    && !defined(NO_GDB)
SUBDIR+=	gdb gdbreplay gdbserver
.endif

.include <bsd.subdir.mk>
