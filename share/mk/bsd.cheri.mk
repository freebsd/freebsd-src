.if defined(NEED_CHERI)
.if ${MK_CHERI} == "no"
.error NEED_CHERI defined, but CHERI is not enabled
.endif
.if ${NEED_CHERI} != "hybrid" && ${NEED_CHERI} != "pure"
.error NEED_CHERI must be 'hybrid' or 'pure'
.endif
WANT_CHERI:= yes
.if ${NEED_CHERI} == "pure"
LIBDIR:=	/usr/libcheri
.endif
.endif

.if ${MK_CHERI} != "no" && defined(WANT_CHERI)
.if !defined(CHERI_CC)
.error CHERI is enabled and request, but CHERI_CC is undefined
.endif
.if !exists(${CHERI_CC}) 
.error CHERI_CC is defined to ${CHERI_CC} which does not exist
.endif

_CHERI_CC=	${CHERI_CC} -integrated-as --target=cheri-unknown-freebsd \
		-msoft-float
.if defined(SYSROOT)
_CHERI_CC+=	--sysroot=${SYSROOT}
.endif
.if defined(NEED_CHERI) && ${NEED_CHERI} == "pure"
_CHERI_CC+=    -mabi=sandbox
.endif
.if ${MK_CHERI128} == "yes"
_CHERI_CC+=	-mllvm -cheri128
# XXX: Needed as Clang rejects -mllvm -cheri128 when using $CC to link.
_CHERI_CFLAGS+=	-Qunused-arguments
.endif

.if ${WANT_CHERI} != "variables"
NO_SHARED=	yes
CC:=	${_CHERI_CC}
CFLAGS+=	${_CHERI_CFLAGS}
# Don't remove CHERI symbols from the symbol table
STRIP_FLAGS+=	-w --keep-symbol=__cheri_callee_method.\* \
		--keep-symbol=__cheri_method.\*
.endif
.endif
