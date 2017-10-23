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

GCNOSOWN?=	${BINOWN}
GCNOSGRP?=	${BINGRP}
GCNOSMODE?=	0644
GCNOSDIRMODE?=	0755

GCNOS:=		${GCNOS:O:u}
FILESGROUPS+=	GCNOS
CLEANFILES+=	${GCNOS}

.for _gcno in ${GCNOS}
_gcno_dir:=		${COVERAGEDIR}${_gcno:H:tA}
GCNOSDIR_${_gcno:T}:=	${_gcno_dir}
# Create _gcno_dir if it doesn't already exist.
.if !target(${DESTDIR}${_gcno_dir})
${DESTDIR}${_gcno_dir}:
	${INSTALL} -d -o ${GCNOSOWN} -g ${GCNOSGRP} -m ${GCNOSDIRMODE} \
	    ${.TARGET}/
beforeinstall: ${DESTDIR}${_gcno_dir}
.endif
.endfor
.endif
