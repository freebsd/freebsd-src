# $FreeBSD$
#
# Snippet for dealing with runtime coverage logic.
#
# .gcno files are generated from files that are compiled from source, e.g.,
# foo.gcno is foo.c or foo.cpp's file. In order for the libraries and programs
# to be properly instrumented, the .gcno files must be installed to a prefix
# common to the object files.
#
# See gcov(1) for more details.

.include <bsd.own.mk>

FILESGROUPS?=	FILES

.if !empty(GCNOS)
GCNOS:=		${GCNOS:O:u}
FILESGROUPS+=	GCNOS
CLEANFILES+=	${GCNOS}

.for _gcno in ${GCNOS}
GCNOSDIR_${_gcno:T}=	${COVERAGEDIR}${_gcno:H:tA}
.endfor
.endif
