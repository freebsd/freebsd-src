# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	doc\
	libiberty \
	libbfd \
	libopcodes \
	libbinutils \
	as \
	ld \
	${_objcopy} \
	objdump \

.if ${MK_ELFCOPY_AS_OBJCOPY} == "no"
_objcopy=	objcopy
.endif

.include <bsd.subdir.mk>
