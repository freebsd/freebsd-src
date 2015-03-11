.if ! exists(${CHERI_CC}) 
.error USE_CHERI is defined and CHERI_CC is ${CHERI_CC}, but it doesn't exist.
.endif
CC:=	${CHERI_CC} -integrated-as --target=cheri-unknown-freebsd -msoft-float
.if defined(SYSROOT)
CC+=	--sysroot=${SYSROOT}
.endif
.if defined(USE_CHERI_STACK)
CC+=    -mabi=sandbox
.endif
