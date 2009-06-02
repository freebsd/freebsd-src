# $FreeBSD$

.include "clang.build.mk"

.for lib in ${OBJDEPS:C/$/.o/} ${LIBDEPS:C/$/.a/}
DPADD+= ${.OBJDIR}/../../lib/lib${lib:C/\..$//}/lib${lib}
LDADD+= ${.OBJDIR}/../../lib/lib${lib:C/\..$//}/lib${lib}
.endfor

BINDIR?=/usr/bin

.include <bsd.prog.mk>
