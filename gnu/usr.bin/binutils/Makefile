#
# $FreeBSD: src/gnu/usr.bin/binutils/Makefile,v 1.11.2.1 2000/07/06 22:15:40 obrien Exp $
#

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as gasp ld nm objcopy objdump ranlib \
		size strings strip doc

.if !defined(NO_GDB)
# Maybe should be unconditional but I don't want to get in the
# way of the mips port.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "alpha"
SUBDIR+=	gdb gdbserver gdbreplay
.endif
.endif

.include <bsd.subdir.mk>
