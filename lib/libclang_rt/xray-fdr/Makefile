.include <bsd.init.mk>

LIB=		clang_rt.xray-fdr-${CRTARCH}

CFLAGS+=	-DSANITIZER_COMMON_NO_REDEFINE_BUILTINS

SRCS+=		xray/xray_fdr_flags.cpp
SRCS+=		xray/xray_fdr_logging.cpp

.include <bsd.lib.mk>
