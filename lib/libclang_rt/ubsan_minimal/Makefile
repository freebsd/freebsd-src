
.include <bsd.init.mk>

LIB=		clang_rt.ubsan_minimal-${CRTARCH}

CFLAGS+=	-DSANITIZER_COMMON_NO_REDEFINE_BUILTINS

SRCS+=		ubsan_minimal/ubsan_minimal_handlers.cpp

.include <bsd.lib.mk>
