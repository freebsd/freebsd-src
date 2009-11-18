# $FreeBSD$

.include "clang.build.mk"

.for lib in ${LIBDEPS}
DPADD+= ${.OBJDIR}/../../lib/lib${lib}/lib${lib}.a
LDADD+= ${.OBJDIR}/../../lib/lib${lib}/lib${lib}.a
.endfor

CXX:=${CXX:C/^c\+\+|^clang\+\+/g++/}

BINDIR?=/usr/bin

.include <bsd.prog.mk>
