# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	libiberty \
	libbfd \
	libopcodes

SUBDIR.${MK_BINUTILS}+=	doc

# GNU as is used on x86 only, for a few files that cannot be assembled by
# Clang IAS. Other archs either use Clang IAS for every assembly file, or
# use external toolchain.
.if (${TARGET} == "amd64" || ${TARGET} == "i386") && \
    (${MK_BINUTILS} != "no" || ${MK_BINUTILS_BOOTSTRAP} != "no")
SUBDIR+=	as
.endif

SUBDIR_DEPEND_as=libbfd libiberty libopcodes

.if !make(install)
SUBDIR_PARALLEL=
.endif

.include <bsd.subdir.mk>
