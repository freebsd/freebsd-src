# $FreeBSD$

SUBDIR= csu libgcc libdialog libregex libreadline

.if ${OBJFORMAT} == aout
.if !defined(NOLIBC_R)
SUBDIR+= libgcc_r
.endif
.endif

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if !defined(NO_CXX)
SUBDIR+= libstdc++ libsupc++
.endif

.if !defined(NO_OBJC)
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>
