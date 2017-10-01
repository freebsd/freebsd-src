# $FreeBSD$
#
# Snippet for dealing with runtime coverage logic.
#
# .gcda files are generated from files that are compiled from source, e.g.,
# foo.gcda is foo.c or foo.cpp's file. In order for the libraries and programs
# to be properly instrumented, the .gcda files must be installed to a prefix
# common to the object files.
#
# See gcov(1) for more details.

.include <bsd.own.mk>

FILESGROUPS?=	FILES

.if !empty(GCDAS)
GCDAS:=		${GCDAS:O:u}
FILESGROUPS+=	GCDAS
CLEANFILES+=	${GCDAS}

.for _gcda in ${GCDAS}
${_gcda}: ${_gcda:.gcda=.o}
GCDASDIR_${_gcda:T}=	${COVERAGEDIR}${_gcda:H:tA}
.endfor
.endif
