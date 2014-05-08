# $FreeBSD$

.if !target(__<bsd.compiler.mk>__)
__<bsd.compiler.mk>__:

.if ${MACHINE} == "common"
COMPILER_TYPE= none
.endif

.if !defined(COMPILER_TYPE)
. if ${CC:T:Mgcc*}
COMPILER_TYPE:=	gcc  
. elif ${CC:T:Mclang}
COMPILER_TYPE:=	clang
. else
_COMPILER_VERSION!=	${CC} --version
.  if ${_COMPILER_VERSION:Mgcc}
COMPILER_TYPE:=	gcc
.  elif ${_COMPILER_VERSION:M\(GCC\)}
COMPILER_TYPE:=	gcc
.  elif ${_COMPILER_VERSION:Mclang}
COMPILER_TYPE:=	clang
.  else
.error Unable to determine compiler type for ${CC}.  Consider setting COMPILER_TYPE.
.  endif
.  undef _COMPILER_VERSION
. endif
.endif

.if ${COMPILER_TYPE} == "clang"
COMPILER_FEATURES=	c++11
.if !defined(_COMPILER_VERSION)
_COMPILER_VERSION!=     ${CC} --version
.endif
# some warnings are version specific
COMPILER_VERSION:= ${_COMPILER_VERSION:M[1-9].[0-9]*}
.else
COMPILER_FEATURES=
.endif

.endif	# !target(__<bsd.compiler.mk>__)
