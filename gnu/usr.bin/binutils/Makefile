# $FreeBSD: src/gnu/usr.bin/binutils/Makefile,v 1.11.2.4 2002/03/06 18:50:27 obrien Exp $

#	In my absence, bde and jdp have pre-approval for any commit they feel
#	necessary.
MAINTAINER=	obrien@FreeBSD.org

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as gasp ld nm objcopy objdump ranlib readelf \
		size strings strip doc

TARGET_ARCH?=	${MACHINE_ARCH}
.if !defined(NO_GDB)
SUBDIR+=	gdb gdbserver gdbreplay
.endif

.include <bsd.subdir.mk>
