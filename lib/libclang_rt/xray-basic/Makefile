.include <bsd.init.mk>

LIB=		clang_rt.xray-basic-${CRTARCH}

CFLAGS+=	-DSANITIZER_COMMON_NO_REDEFINE_BUILTINS

SRCS+=		xray/xray_basic_flags.cpp
SRCS+=		xray/xray_basic_logging.cpp

.include <bsd.lib.mk>
