#
# Rules for building programs.
#
# $Id: elftoolchain.prog.mk 3652 2018-12-23 07:59:42Z jkoshy $

.if !defined(TOP)
.error	Make variable \"TOP\" has not been defined.
.endif

.include "${TOP}/mk/elftoolchain.os.mk"

LIBDWARF?=	${TOP}/libdwarf
LIBELF?=	${TOP}/libelf
LIBELFTC?=	${TOP}/libelftc

BINDIR?=	/usr/bin

CFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/${TOP}/common
CLEANFILES+=	.depend

# TODO[#271]: Reduce the code duplication below.

.if defined(LDADD)
_LDADD_LIBDWARF=${LDADD:M-ldwarf}
.if !empty(_LDADD_LIBDWARF)
CFLAGS+= -I${.CURDIR}/${TOP}/libdwarf
.if exists(${.OBJDIR}/${TOP}/libdwarf)
LDFLAGS+= -L${.OBJDIR}/${TOP}/libdwarf
.elif exists(${TOP}/libdwarf/${.OBJDIR:S,${.CURDIR}/,,})
LDFLAGS+= -L${.CURDIR}/${TOP}/libdwarf/${.OBJDIR:S,${.CURDIR}/,,}
.else
.error Cannot determine LDFLAGS for -ldwarf.
.endif
.endif

_LDADD_LIBELF=${LDADD:M-lelf}
.if !empty(_LDADD_LIBELF)
CFLAGS+= -I${.CURDIR}/${TOP}/libelf
.if exists(${.OBJDIR}/${TOP}/libelf)
LDFLAGS+= -L${.OBJDIR}/${TOP}/libelf
.elif exists(${TOP}/libelf/${.OBJDIR:S,${.CURDIR}/,,})
LDFLAGS+= -L${.CURDIR}/${TOP}/libelf/${.OBJDIR:S,${.CURDIR}/,,}
.else
.error Cannot determine LDFLAGS for -lelf.
.endif
.endif

_LDADD_LIBELFTC=${LDADD:M-lelftc}
.if !empty(_LDADD_LIBELFTC)
CFLAGS+= -I${.CURDIR}/${TOP}/libelftc
.if exists(${.OBJDIR}/${TOP}/libelftc)
LDFLAGS+= -L${.OBJDIR}/${TOP}/libelftc
.elif exists(${TOP}/libelftc/${.OBJDIR:S,${.CURDIR}/,,})
LDFLAGS+= -L${.CURDIR}/${TOP}/libelftc/${.OBJDIR:S,${.CURDIR}/,,}
.else
.error Cannot determine LDFLAGS for -lelftc.
.endif
.endif

_LDADD_LIBPE=${LDADD:M-lpe}
.if !empty(_LDADD_LIBPE)
CFLAGS+= -I${.CURDIR}/${TOP}/libpe
.if exists(${.OBJDIR}/${TOP}/libpe)
LDFLAGS+= -L${.OBJDIR}/${TOP}/libpe
.elif exists(${TOP}/libpe/${.OBJDIR:S,${.CURDIR}/,,})
LDFLAGS+= -L${.CURDIR}/${TOP}/libpe/${.OBJDIR:S,${.CURDIR}/,,}
.else
.error Cannot determine LDFLAGS for -lpe.
.endif
.endif
.endif

_LDADD_LIBARCHIVE=${LDADD:M-larchive}
.if !empty(_LDADD_LIBARCHIVE) && ${OS_HOST} == NetBSD
CFLAGS+=	-I/usr/pkg/include
LDFLAGS+=	-L/usr/pkg/lib
.endif

#
# Handle lex(1) and yacc(1) in a portable fashion.
#
# New makefile variables used:
#
# LSRC		-- a lexer definition suitable for use with lex(1)
# YSRC		-- a parser definition for use with yacc(1)

# Use standard rules from <bsd.*.mk> for building lexers.
.if defined(LSRC)
SRCS+=	${LSRC}
.endif

# Handle the generation of yacc based parsers.
# If the program uses a lexer, add an automatic dependency
# on the generated parser header.
.if defined(YSRC)
.for _Y in ${YSRC}
SRCS+=	${_Y:R}.c
CLEANFILES+=	${_Y:R}.c ${_Y:R}.h
${_Y:R}.c ${_Y:R}.h:	${_Y}
	${YACC} -d -o ${_Y:R}.c ${.ALLSRC}

.if defined(LSRC)
.for _L in ${LSRC}
${_L:R}.o:	${_Y:R}.h
.endfor
.endif

.endfor
.endif

.include <bsd.prog.mk>

# Note: include the M4 ruleset after bsd.prog.mk.
.include "${TOP}/mk/elftoolchain.m4.mk"

# Support a 'clobber' target.
clobber:	clean os-specific-clobber .PHONY

.if defined(DEBUG)
CFLAGS:=	${CFLAGS:N-O*} -g
.endif

# Bring in rules related to running the related test suite.
.include "${TOP}/mk/elftoolchain.test-target.mk"
