# $FreeBSD$

.if ${MK_DIRDEPS_BUILD} == "yes"
MAKE_PRINT_VAR_ON_ERROR+= \
	.CURDIR \
	.MAKE \
	.OBJDIR \
	.TARGETS \
	DESTDIR \
	LD_LIBRARY_PATH \
	MACHINE \
	MACHINE_ARCH \
	MAKEOBJDIRPREFIX \
	MAKESYSPATH \
	MAKE_VERSION\
	PATH \
	SRCTOP \
	OBJTOP \
	${MAKE_PRINT_VAR_ON_ERROR_XTRAS}

.if ${.MAKE.LEVEL} > 0
MAKE_PRINT_VAR_ON_ERROR += .MAKE.MAKEFILES .PATH
.endif
.endif

.include "src.sys.mk"

.if ${.MAKE.MODE:Mmeta*} != ""
# we can afford to use cookies to prevent some targets
# re-running needlessly but only when using filemon.
.if ${.MAKE.MODE:Mnofilemon} == ""
META_COOKIE=		${COOKIE.${.TARGET}:U${.OBJDIR}/${.TARGET}}
META_COOKIE_RM=		@rm -f ${META_COOKIE}
META_COOKIE_TOUCH=	@touch ${META_COOKIE}
CLEANFILES+=		${META_TARGETS}
_meta_dep_before:	.USEBEFORE
	${META_COOKIE_RM}
_meta_dep_after:	.USE
	${META_COOKIE_TOUCH}
# Attach this to a target to allow it to benefit from meta mode's
# not rerunning a command if it doesn't need to be considering its
# metafile/filemon-tracked dependencies.
META_DEPS=	_meta_dep_before _meta_dep_after .META
.endif
.else
# some targets need to be .PHONY - but not in meta mode
META_NOPHONY=	.PHONY
.endif
META_NOPHONY?=
META_COOKIE_RM?=
META_COOKIE_TOUCH?=
META_DEPS+=	${META_NOPHONY}
