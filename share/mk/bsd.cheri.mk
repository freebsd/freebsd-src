.if !${.TARGETS:Mbuild-tools} && !defined(BOOTSTRAPPING)
.if defined(NEED_CHERI)
.if ${MK_CHERI} == "no"
.error NEED_CHERI defined, but CHERI is not enabled
.endif
.if ${NEED_CHERI} != "hybrid" && ${NEED_CHERI} != "pure" && ${NEED_CHERI} != "sandbox"
.error NEED_CHERI must be 'hybrid', 'pure', or 'sandbox'
.endif
.if defined(WHAT_CHERI)
.error WANT_CHERI should not be defined in NEED_CHERI is
.endif
WANT_CHERI:= ${NEED_CHERI}
.endif

.if ${MK_CHERI} != "no" && defined(WANT_CHERI) && ${WANT_CHERI} != "none"
.if !defined(CHERI_CC)
.error CHERI is enabled and request, but CHERI_CC is undefined
.endif
.if !exists(${CHERI_CC}) 
.error CHERI_CC is defined to ${CHERI_CC} which does not exist
.endif

_CHERI_CC=	${CHERI_CC} -g -integrated-as --target=cheri-unknown-freebsd \
		-msoft-float
.if defined(SYSROOT)
_CHERI_CC+=	--sysroot=${SYSROOT}
.endif

.if ${WANT_CHERI} == "pure" || ${WANT_CHERI} == "sandbox"
OBJCOPY:=	objcopy
_CHERI_CC+=	-mabi=sandbox -mxgot
LIBDIR:=	/usr/libcheri
ROOTOBJDIR=	${.OBJDIR:S,${.CURDIR},,}${SRCTOP}/worldcheri${SRCTOP}
CFLAGS+=	-O2 -ftls-model=local-exec
.if ${MK_CHERI_LINKER} == "yes"
_CHERI_CC+=	-cheri-linker
CFLAGS+=	-Wno-error
.endif
ALLOW_SHARED_TEXTREL=	yes
.ifdef LIBCHERI
LDFLAGS+=	-Wl,-init=crt_init_globals
.endif
.endif

.if ${MK_CHERI128} == "yes"
_CHERI_CC+=	-mllvm -cheri128
# XXX: Needed as Clang rejects -mllvm -cheri128 when using $CC to link.
_CHERI_CFLAGS+=	-Qunused-arguments
.endif

.if ${WANT_CHERI} != "variables"
.if ${MK_CHERI_SHARED} == "no"
NO_SHARED=	yes
.elif defined(__BSD_PROG_MK) && ${MK_CHERI_SHARED_PROG} == "no"
NO_SHARED=	yes
.endif
CC:=	${_CHERI_CC}
COMPILER_TYPE=	clang
CFLAGS+=	${_CHERI_CFLAGS}
# Don't remove CHERI symbols from the symbol table
STRIP_FLAGS+=	-w --keep-symbol=__cheri_callee_method.\* \
		--keep-symbol=__cheri_method.\*
.endif
.endif
.endif
