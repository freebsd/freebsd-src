# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	doc\
	libiberty \
	libbfd \
	libopcodes \
	libbinutils \
	.WAIT \
	as \
	ld \
	${_objcopy} \
	objdump \

.if ${MK_ELFCOPY_AS_OBJCOPY} == "no"
_objcopy=	objcopy
.endif


.if !make(install)
SUBDIR_PARALLEL=
.endif

.include <bsd.subdir.mk>
