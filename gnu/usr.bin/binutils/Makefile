# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	doc\
	libiberty \
	libbfd \
	libopcodes \
	libbinutils \
	${_addr2line} \
	as \
	ld \
	${_nm} \
	${_objcopy} \
	objdump \
	${_readelf} \
	${_size} \
	${_strings} \
	${_strip}

.if ${MK_ELFTOOLCHAIN_TOOLS} == "no"
_addr2line=	addr2line
_nm=		nm
_readelf=	readelf
_size=		size
_strings=	strings
_strip=		strip
.endif
.if ${MK_ELFTOOLCHAIN_TOOLS} == "no" || ${MK_ELFCOPY_AS_OBJCOPY} == "no"
_objcopy=	objcopy
.endif

.include <bsd.subdir.mk>
