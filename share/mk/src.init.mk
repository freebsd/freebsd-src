
.if !target(__<src.init.mk>__)
__<src.init.mk>__:	.NOTMAIN

.if !target(buildenv)
buildenv: .PHONY .NOTMAIN
	${_+_}@env BUILDENV_DIR=${.CURDIR} ${MAKE} -C ${SRCTOP} buildenv
.endif

.if ${MACHINE:Nhost*} == ""
.if ${.MAKE.OS} != "FreeBSD"
# these won't work anyway - see tools/build/mk/Makefile.boot.pre
MK_DEBUG_FILES= no
MK_MAN= no
MK_PIE= no
MK_RETPOLINE= no
NO_SHARED= no
MK_TESTS= no

.-include <src.init.${.MAKE.OS:tl}.mk>

CFLAGS+= \
	-DHAVE_NBTOOL_CONFIG_H=1 \
	-I${SRCTOP}/tools/build/cross-build/include/common \

.endif

.if ${MK_host_egacy} == "yes"
.ifdef PROG
LOCAL_LIBRARIES+= egacy
LIBADD+= egacy
.endif
.endif

.endif

.endif	# !target(__<src.init.mk>__)
