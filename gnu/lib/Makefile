# $FreeBSD$

.if ${MACHINE_ARCH} != "ia64" && ${MACHINE_ARCH} != "sparc64"
SUBDIR+= csu libgcc
.else
NO_CXX=	yes
NO_OBJC=yes
NO_FORTRAN=yes
.endif

SUBDIR+= libdialog libregex libreadline

.if ${OBJFORMAT} == aout
.if !defined(NOLIBC_R)
SUBDIR+= libgcc_r
.endif
.endif

.if !defined(NO_CXX)
SUBDIR+= libstdc++
.endif

.if !defined(NO_OBJC)
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>
