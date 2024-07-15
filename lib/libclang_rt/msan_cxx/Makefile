.include <bsd.init.mk>

LIB=		clang_rt.msan_cxx-${CRTARCH}

SRCS+=		msan/msan_new_delete.cpp
SRCS+=		ubsan/ubsan_handlers_cxx.cpp
SRCS+=		ubsan/ubsan_type_hash.cpp
SRCS+=		ubsan/ubsan_type_hash_itanium.cpp

.include <bsd.lib.mk>

CXXFLAGS.ubsan_handlers_cxx.cpp=		-frtti
CXXFLAGS.ubsan_type_hash.cpp=		-frtti
CXXFLAGS.ubsan_type_hash_itanium.cpp=	-frtti
