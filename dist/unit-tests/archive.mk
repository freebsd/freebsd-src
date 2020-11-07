# $NetBSD: archive.mk,v 1.10 2020/10/09 06:44:42 rillig Exp $
#
# Very basic demonstration of handling archives, based on the description
# in PSD.doc/tutorial.ms.
#
# This test aims at covering the code, not at being an introduction to
# archive handling. That's why it is more complicated and detailed than
# strictly necessary.

ARCHIVE=	libprog.a
FILES=		archive.mk modmisc.mk varmisc.mk

MAKE_CMD=	${.MAKE} -f ${MAKEFILE}
RUN?=		@set -eu;

all:
.if ${.PARSEDIR:tA} != ${.CURDIR:tA}
	@cd ${MAKEFILE:H} && cp ${FILES} [at]*.mk ${.CURDIR}
.endif
# The following targets create and remove files.  The filesystem cache in
# dir.c would probably not handle this correctly, therefore each of the
# targets is run in its separate sub-make.
	${RUN} ${MAKE_CMD} remove-archive
	${RUN} ${MAKE_CMD} create-archive
	${RUN} ${MAKE_CMD} list-archive
	${RUN} ${MAKE_CMD} list-archive-wildcard
	${RUN} ${MAKE_CMD} depend-on-existing-member
	${RUN} ${MAKE_CMD} depend-on-nonexistent-member
	${RUN} ${MAKE_CMD} remove-archive

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
# To prevent an overly long file list, the pattern is restricted to [at]*.mk.
list-archive-wildcard: ${ARCHIVE}([at]*.mk) pre post
	${RUN} printf '%s\n' ${.ALLSRC:O:@member@${.TARGET:Q}': '${member:Q}@}

depend-on-existing-member: ${ARCHIVE}(archive.mk) pre post
	${RUN} echo $@

depend-on-nonexistent-member: ${ARCHIVE}(nonexistent.mk) pre post
	${RUN} echo $@

remove-archive: pre post
	rm -f ${ARCHIVE}

pre: .USEBEFORE
	@echo Making ${.TARGET} ${.OODATE:C,.+,out-of-date,W} ${.OODATE:O}
post: .USE
	@echo
