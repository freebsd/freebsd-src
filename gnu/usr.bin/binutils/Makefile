#
# $Id: Makefile,v 1.6 1999/05/02 11:38:12 dfr Exp $
#

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as ld nm objcopy objdump ranlib \
		size strings strip doc

# Maybe should be unconditional but I don't want to get in the
# way of the mips port.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "alpha"
SUBDIR+=	gdb
.endif

.include <bsd.subdir.mk>
