#
# $FreeBSD$
#

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as gasp ld nm objcopy objdump ranlib \
		size strings strip doc

# Maybe should be unconditional but I don't want to get in the
# way of the mips port.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "alpha"
SUBDIR+=	gdb gdbserver gdbreplay
.endif

.include <bsd.subdir.mk>
