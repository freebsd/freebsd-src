# $FreeBSD$

.include "clang.build.mk"

.if defined(MAKEOBJ)
# XXX: In some cases we cannot use archives, such as the targets.
INTERNALPROG=
PROG=	lib${LIB}.o
LDADD=	-Wl,-r -nodefaultlibs -nostdlib -nostartfiles
NO_MAN=
.include <bsd.prog.mk>
.else
INTERNALLIB=
.include <bsd.lib.mk>
.endif
