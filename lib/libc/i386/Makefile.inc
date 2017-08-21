# $FreeBSD$

# Long double is 80 bits
GDTOASRCS+=strtorx.c
SRCS+=machdep_ldisx.c
SYM_MAPS+=${LIBC_SRCTOP}/i386/Symbol.map

# XXX force use of ld.bfd for linking i386 libc
#
# lld can successfully link most of a working i386 userland and kernel,
# but produces a broken libc. For now if we're otherwise using lld, and
# ld.bfd is available, explicitly use it for libc.
.include <bsd.linker.mk>
.if ${LINKER_TYPE} == "lld" && ${MK_BINUTILS_BOOTSTRAP} != "no"
LDFLAGS+=-fuse-ld=bfd
.endif
