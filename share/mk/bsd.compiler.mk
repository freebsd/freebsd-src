# $FreeBSD$

.if !defined(COMPILER_TYPE)
. if ${CC:T:Mgcc} == "gcc"
COMPILER_TYPE:=	gcc  
. elif ${CC:T:Mclang} == "clang"
COMPILER_TYPE:=	clang
. else
_COMPILER_VERSION!=	${CC} --version
.  if ${_COMPILER_VERSION:Mgcc} == "gcc"
COMPILER_TYPE:=	gcc
.  elif ${_COMPILER_VERSION:M\(GCC\)} == "(GCC)"
COMPILER_TYPE:=	gcc
.  elif ${_COMPILER_VERSION:Mclang} == "clang"
COMPILER_TYPE:=	clang
.  else
.error Unable to determing compiler type for ${CC}
.  endif
.  undef _COMPILER_VERSION
. endif
.endif
