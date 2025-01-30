# $NetBSD: archive.mk,v 1.14 2025/01/10 23:00:38 rillig Exp $
#
# Very basic demonstration of handling archives, based on the description
# in PSD.doc/tutorial.ms.
#
# This test aims at covering the code, not at being an introduction to
# archive handling. That's why it deviates from the tutorial style of
# several other tests.

ARCHIVE=	libprog.a
FILES=		archive.mk archive-suffix.mk modmisc.mk ternary.mk varmisc.mk

all:
.if ${.PARSEDIR:tA} != ${.CURDIR:tA}
	@cd ${MAKEFILE:H} && cp ${FILES} ${.CURDIR}
.endif
# The following targets create and remove files.  The filesystem cache in
# dir.c would probably not handle this correctly, therefore each of the
# targets is run in its separate sub-make.
	@${MAKE} -f ${MAKEFILE} remove-archive
	@${MAKE} -f ${MAKEFILE} create-archive
	@${MAKE} -f ${MAKEFILE} list-archive
	@${MAKE} -f ${MAKEFILE} list-archive-wildcard
	@${MAKE} -f ${MAKEFILE} list-archive-undef-archive || echo "exit $$?"
	@echo
	@${MAKE} -f ${MAKEFILE} list-archive-undef-member || echo "exit $$?"
	@echo
	@${MAKE} -f ${MAKEFILE} depend-on-existing-member
	@${MAKE} -f ${MAKEFILE} depend-on-nonexistent-member
	@${MAKE} -f ${MAKEFILE} remove-archive
	@${MAKE} -f ${MAKEFILE} set-up-library
	@${MAKE} -f ${MAKEFILE} -dm library 2>&1 \
	| sed -n '/^Examining/p' \
	| sed 's,\.\.\.modified[^.]*,,'
	@${MAKE} -f ${MAKEFILE} tear-down-library


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

.if make(list-archive-undef-archive)
# TODO: Be more specific: mention that the variable "UNDEF" is not defined.
# expect+1: Error in source archive spec "libprog.a${UNDEF}(archive.mk) pre post"
list-archive-undef-archive: ${ARCHIVE}$${UNDEF}(archive.mk) pre post
	@printf '%s\n' ${.ALLSRC:O:@member@${.TARGET:Q}': '${member:Q}@}
.endif

.if make(list-archive-undef-member)
# TODO: Be more specific: mention that the variable "UNDEF" is not defined.
# expect+1: Error in source archive spec "libprog.a"
list-archive-undef-member: ${ARCHIVE}(archive$${UNDEF}.mk) pre post
	@printf '%s\n' ${.ALLSRC:O:@member@${.TARGET:Q}': '${member:Q}@}
.endif

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


set-up-library: .PHONY
	@echo "member" > member.txt
	@echo "not a library" > libbad.a
	@ar cr libgood.a member.txt
	@echo "begin library"

.if make(library)
.SUFFIXES: .a
.LIBS: .a
.endif
# The two lines for libgood contain the word "library", the two lines for
# libbad don't.
#
# expect: Examining libbad.a...up-to-date.
# expect: Examining -lbad...up-to-date.
# expect: Examining libgood.a...library...up-to-date.
# expect: Examining -lgood...library...up-to-date.
library: .PHONY libbad.a -lbad libgood.a -lgood
	: Making ${.TARGET} from ${.ALLSRC}

tear-down-library: .PHONY
	@echo "end library"
	@rm member.txt libbad.a libgood.a
