# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	libiberty \
	libbfd \
	libopcodes

SUBDIR.${MK_BINUTILS}+=	doc
SUBDIR.${MK_BINUTILS}+=	libbinutils
SUBDIR.${MK_BINUTILS}+=	objdump

# GNU as is used on x86 only, for a few files that cannot be assembled by
# Clang IAS. Other archs either use Clang IAS for every assembly file, or
# use external toolchain.
.if ${TARGET} == "amd64" || ${TARGET} == "i386"
SUBDIR.${MK_BINUTILS}+=	as
.endif

SUBDIR_DEPEND_libbinutils=libbfd		# for bfdver.h
SUBDIR_DEPEND_as=libbfd libiberty libopcodes
SUBDIR_DEPEND_ld=libbfd libiberty
SUBDIR_DEPEND_objdump=libbfd libiberty libbinutils libopcodes

.if !make(install)
SUBDIR_PARALLEL=
.endif

.include <bsd.subdir.mk>
