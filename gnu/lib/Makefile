# $FreeBSD$

SUBDIR= libdialog libgcc libgcc_r libgmp libmp libregex libreadline libstdc++

.if !defined(NO_OBJC)
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>
