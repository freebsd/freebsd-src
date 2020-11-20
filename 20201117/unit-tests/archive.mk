# $NetBSD: archive.mk,v 1.11 2020/11/15 14:07:53 rillig Exp $
#
# Very basic demonstration of handling archives, based on the description
# in PSD.doc/tutorial.ms.
#
# This test aims at covering the code, not at being an introduction to
# archive handling. That's why it deviates from the tutorial style of
# several other tests.

ARCHIVE=	libprog.a
FILES=		archive.mk modmisc.mk varmisc.mk

all:
.if ${.PARSEDIR:tA} != ${.CURDIR:tA}
	@cd ${MAKEFILE:H} && cp ${FILES} [at]*.mk ${.CURDIR}
.endif
# The following targets create and remove files.  The filesystem cache in
# dir.c would probably not handle this correctly, therefore each of the
# targets is run in its separate sub-make.
	@${MAKE} -f ${MAKEFILE} remove-archive
	@${MAKE} -f ${MAKEFILE} create-archive
	@${MAKE} -f ${MAKEFILE} list-archive
	@${MAKE} -f ${MAKEFILE} list-archive-wildcard
	@${MAKE} -f ${MAKEFILE} depend-on-existing-member
	@${MAKE} -f ${MAKEFILE} depend-on-nonexistent-member
	@${MAKE} -f ${MAKEFILE} remove-archive

create-archive: ${ARCHIVE} pre post

# The indirect references with the $$ cover the code in Arch_ParseArchive
# that calls Var_Parse.  It's an esoteric scenario since at the point where
# Arch_ParseArchive is called, the dependency line is already fully expanded.
#
${ARCHIVE}: $${:Ulibprog.a}(archive.mk modmisc.mk $${:Uvarmisc.mk}) pre post
	ar cru ${.TARGET} ${.OODATE:O}
	ranlib ${.TARGET}

list-archive: ${ARCHIVE} pre post
	ar t ${.ALLSRC}

# XXX: I had expected that this dependency would select all *.mk files from
# the archive.  Instead, the globbing is done in the current directory.
#
# To prevent an overly long file list, the pattern is restricted to [at]*.mk.
list-archive-wildcard: ${ARCHIVE}([at]*.mk) pre post
	@printf '%s\n' ${.ALLSRC:O:@member@${.TARGET:Q}': '${member:Q}@}

depend-on-existing-member: ${ARCHIVE}(archive.mk) pre post
	@echo $@

depend-on-nonexistent-member: ${ARCHIVE}(nonexistent.mk) pre post
	@echo $@

remove-archive: pre post
	rm -f ${ARCHIVE}

pre: .USEBEFORE
	@echo Making ${.TARGET} ${.OODATE:C,.+,out-of-date,W} ${.OODATE:O}
post: .USE
	@echo
