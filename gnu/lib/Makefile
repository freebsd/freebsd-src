# $FreeBSD$

SUBDIR= csu libdialog libgcc libgcc_r libgmp libmp libregex libreadline

.if !defined(NOLIBC_R)
SUBDIR+= libgcc_r
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
