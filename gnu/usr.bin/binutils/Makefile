#
# $FreeBSD: src/gnu/usr.bin/binutils/Makefile,v 1.4.2.2 1999/08/29 14:31:50 peter Exp $
#

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as ld nm objcopy objdump ranlib \
		size strings strip doc

# Maybe should be unconditional but I don't want to get in the
# way of the mips port.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "alpha"
SUBDIR+=	gdb gdbserver gdbreplay
.endif

.include <bsd.subdir.mk>
