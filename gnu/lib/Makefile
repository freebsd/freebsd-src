# $FreeBSD$

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR= libdialog libregex libreadline
.else
SUBDIR= csu libdialog libgcc libregex libreadline
.endif

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
