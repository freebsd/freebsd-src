# $Id: Makefile 2563 2012-09-01 14:31:42Z jkoshy $
#
# The same 'source' for a literate program is used to build both
# documentation and the program itself.
#
# Keeping the build rules for program code and documentation in
# separate Makefiles help to keep each rule set simple.

TOP=		../..

# The literate program input, in order of presentation in the
# generated text.
NW_SRCS= 	introduction.nw
NW_SRCS+= 	userguide.nw
NW_SRCS+=	implementation.nw
NW_SRCS+=	slave.nw
NW_SRCS+=	master.nw
NW_SRCS+=	utilities.nw

NW_ROOTS!=	noroots ${NW_SRCS} /dev/null | sed -e 's:<<::' -e 's:>>::'

# The name of the document's base file.
DOC=		build-system

PROGRAMDIRS=	master slave

SUBMAKEFILES=	documentation manuals program

# Declare make targets with special characteristics.
.MAIN:		all
.PHONY:		all clean clobber index install test

# Capture actions shared by multiple targets in a macro.
.dosubmake:	.USE
.for rules in ${SUBMAKEFILES}
	@${MAKE} -f Makefile.${rules} ${.TARGET} NW_SRCS="${NW_SRCS}" \
		DOC=${DOC} TOP="${TOP}"
.endfor

# Standard targets.
all:		${PROGRAMDIRS} ${NW_ROOTS} ${DOC}.tex .dosubmake
install:	all .dosubmake
clean:		.dosubmake
	rm -f ${CLEANFILES}

# Create the directories holding the program source code, if needed.
${PROGRAMDIRS}:	.SILENT
	[ -d ${.TARGET} ] || mkdir ${.TARGET}

# Extract all program 'root chunks' from the literate program inputs.
.for n in ${NW_ROOTS}
$n:	${NW_SRCS}
	@tmpfile=`mktemp /tmp/mba.XXXXXXX`; \
	trap "rm $${tmpfile}" 0 1 2 3 15; \
	notangle -L -R${.TARGET} ${NW_SRCS} > $${tmpfile} || exit $$?; \
	cpif ${.TARGET} < $${tmpfile}
CLEANFILES+=	$n
.endfor

# Create a single TeX file holding the material to be typeset.
# By passing the complete literate program as input to 'noweave', we
# avoid noweb's limitation on chunk names having file scope.
${DOC}.tex:	 ${DOC}.pre.nw ${NW_SRCS} ${DOC}.post.nw
	cat ${.ALLSRC} | noweave -delay -index | cpif ${.TARGET}

#
# Special targets.
#

# The 'index' target only applies to the documentation.
index:
	@${MAKE} -f Makefile.documentation ${.TARGET} NW_SRCS="${NW_SRCS}" \
		TOP="${TOP}"

# The 'test' target only applies to the programs.
test:
	@${MAKE} -f Makefile.program ${.TARGET} NW_SRCS="${NW_SRCS}" \
		TOP="${TOP}"

# Provide a 'clobber' target that goes beyond 'clean'.
clobber: clean
	rm -rf ${PROGRAMDIRS}
