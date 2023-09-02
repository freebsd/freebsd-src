
.include <bsd.init.mk>

LIB=		clang_rt.xray-profiling-${CRTARCH}

CFLAGS+=	-DSANITIZER_COMMON_NO_REDEFINE_BUILTINS

SRCS+=		xray/xray_profile_collector.cpp
SRCS+=		xray/xray_profiling.cpp
SRCS+=		xray/xray_profiling_flags.cpp

.include <bsd.lib.mk>
