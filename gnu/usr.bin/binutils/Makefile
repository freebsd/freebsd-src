# $FreeBSD$

#	In my absence, bde and jdp have pre-approval for any commit they feel
#	necessary.
MAINTAINER=	obrien@FreeBSD.org

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as gasp ld nm objcopy objdump ranlib readelf \
		size strings strip doc

.if !defined(NO_GDB)
# Maybe should be unconditional but I don't want to get in the
# way of the mips port.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "alpha"
SUBDIR+=	gdb gdbserver gdbreplay
.endif
.endif

.include <bsd.subdir.mk>
