.include <bsd.init.mk>

LIB=		clang_rt.asan_cxx-${CRTARCH}

CXXFLAGS+=	-frtti

SRCS+=		asan/asan_new_delete.cpp
SRCS+=		ubsan/ubsan_handlers_cxx.cpp
SRCS+=		ubsan/ubsan_type_hash.cpp
SRCS+=		ubsan/ubsan_type_hash_itanium.cpp

.include <bsd.lib.mk>
