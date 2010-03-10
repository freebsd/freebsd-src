# $FreeBSD$

.include "clang.build.mk"

.for lib in ${LIBDEPS}
DPADD+= ${.OBJDIR}/../../lib/lib${lib}/lib${lib}.a
LDADD+= ${.OBJDIR}/../../lib/lib${lib}/lib${lib}.a
.endfor

BINDIR?=/usr/bin

.include <bsd.prog.mk>
