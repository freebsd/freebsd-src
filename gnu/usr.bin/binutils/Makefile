#
# $Id: Makefile,v 1.5 1999/04/18 10:25:57 obrien Exp $
#

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as ld nm objcopy objdump ranlib \
		size strings strip doc

# Short term measure until I can get CVS to resurrect a file for alpha.
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	gdb
.endif

.include <bsd.subdir.mk>
