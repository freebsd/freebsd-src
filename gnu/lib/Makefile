# $FreeBSD$

SUBDIR= csu libgcc libdialog libregex libreadline

.if ${OBJFORMAT} == aout
.if !defined(NOLIBC_R)
SUBDIR+= libgcc_r
.endif
.endif

.if ${CXX:T} == "c++"
NO_CXX=	yes
.endif

.if !defined(NO_CXX)
SUBDIR+= libsupc++ libstdc++
.endif

.if !defined(NO_OBJC)
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>
