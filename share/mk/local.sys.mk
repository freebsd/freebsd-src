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

.if ${.MAKE.MODE:Unormal:Mmeta*} != ""
# we can afford to use cookies to prevent some targets
# re-running needlessly
META_COOKIE_TOUCH= touch ${COOKIE.${.TARGET}:U${.OBJDIR}/${.TARGET}}
# some targets need to be .PHONY - but not in meta mode
META_NOPHONY=
.else
META_COOKIE_TOUCH=
META_NOPHONY= .PHONY
.endif

