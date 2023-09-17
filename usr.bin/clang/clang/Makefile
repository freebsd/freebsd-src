
.include <src.opts.mk>

PROG_CXX=	clang

SRCDIR=		clang/tools/driver
SRCS+=		cc1_main.cpp
SRCS+=		cc1as_main.cpp
SRCS+=		cc1gen_reproducer_main.cpp
SRCS+=		driver.cpp

CFLAGS.driver.cpp+=	-Dclang_main=main

LINKS=	${BINDIR}/clang ${BINDIR}/clang++ \
	${BINDIR}/clang ${BINDIR}/clang-cpp
MLINKS=	clang.1 clang++.1 \
	clang.1 clang-cpp.1

SCRIPTS=CC.sh
SCRIPTSNAME=CC

LINKS+=	${BINDIR}/clang ${BINDIR}/cc \
	${BINDIR}/clang ${BINDIR}/c++ \
	${BINDIR}/clang ${BINDIR}/cpp
MLINKS+= clang.1 cc.1 \
	clang.1 c++.1 \
	clang.1 CC.1 \
	clang.1 cpp.1

# Ensure we don't add CFLAGS.clang when using GCC
NO_TARGET_FLAGS=

.include "../clang.prog.mk"
