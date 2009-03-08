# $FreeBSD$
#
# Built files
#
# Any headers in the source file list are built
BUILTFILES+= ${SRCS:M*.h}

# Any generated source files are built
GENSRCS?=
GENHDRS?=
BUILTFILES+= ${GENSRCS} ${GENHDRS}

# The directory dependencies must be complete before the generated
# sources and headers are built.
.for _S in ${GENSRCS} ${GENHDRS}
.ORDER: dirdep ${_S}
.endfor

# Any yacc files in the source file list are built into a C file and
# a corresponding header
.for _S in ${SRCS:M*.y} ${GENSRCS:M*.y}
BUILTFILES+= ${_S:R}.h ${_S:R}.c
.endfor

# Rules to build the C file and header from yacc source
.for _S in ${SRCS:M*.y} ${GENSRCS:M*.y}
${_S:R}.h: ${_S}
	${YACC} ${YFLAGS} -d -b ${_S:R} ${.ALLSRC}
	mv ${_S:R}.tab.h ${_S:R}.h
	mv ${_S:R}.tab.c ${_S:R}.c
${_S:R}.c: ${_S:R}.h
.ORDER: dirdep ${_S:R}.h ${_S:R}.c
.endfor

# Any lex files in the source file list are built into a C file
.for _S in ${SRCS:M*.l} ${GENSRCS:M*.l}
BUILTFILES+= ${_S:R}.c
.endfor

# Rules to build the C file from lex source
.for _S in ${SRCS:M*.l} ${GENSRCS:M*.l}
${_S:R}.c: ${_S}
	${LEX} ${LFLAGS} -o${.TARGET} ${.ALLSRC}
.ORDER: dirdep ${_S:R}.c
.endfor

# Any m4 files in the source file list are built into a C file
.for _S in ${SRCS:M*.m4}
BUILTFILES+= ${_S:R}.c
.endfor

.for _S in ${SRCS:M*.m4}
.ORDER: dirdep ${_S:R}.c
.endfor

# Any error table files in the source file list are built into a C file and
# a corresponding header
.for _S in ${SRCS:M*.et}
BUILTFILES+= ${_S:R}.h ${_S:R}.c
.endfor

# Rules to build the C file and header from error table source
.for _S in ${SRCS:M*.et}
${_S:R}.c ${_S:R}.h: ${_S}
	compile_et ${.ALLSRC}
.endfor

genfiles: ${BUILTFILES}
