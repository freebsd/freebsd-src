# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	libiberty \
	libbfd \
	libopcodes

SUBDIR.${MK_BINUTILS}+=	doc
SUBDIR.${MK_BINUTILS}+=	libbinutils
SUBDIR.${MK_BINUTILS}+=	as
SUBDIR.${MK_BINUTILS}+=	objdump

# All archs except powerpc either use lld or require external toolchain.
# powerpc still needs binutils ld to link 32-bit binaries.
.if ${TARGET} == "powerpc"
SUBDIR.${MK_BINUTILS}+=ld
.endif

SUBDIR_DEPEND_libbinutils=libbfd		# for bfdver.h
SUBDIR_DEPEND_as=libbfd libiberty libopcodes
SUBDIR_DEPEND_ld=libbfd libiberty
SUBDIR_DEPEND_objdump=libbfd libiberty libbinutils libopcodes

.if !make(install)
SUBDIR_PARALLEL=
.endif

.include <bsd.subdir.mk>
