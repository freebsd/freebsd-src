# $NetBSD: archive.mk,v 1.5 2020/08/23 17:51:24 rillig Exp $
#
# Very basic demonstration of handling archives, based on the description
# in PSD.doc/tutorial.ms.

ARCHIVE=	libprog.${EXT.a}
FILES=		archive.${EXT.mk} modmisc.${EXT.mk} varmisc.mk

EXT.a=		a
EXT.mk=		mk

MAKE_CMD=	${.MAKE} -f ${MAKEFILE}
RUN?=		@set -eu;

all:
	${RUN} ${MAKE_CMD} remove-archive
	${RUN} ${MAKE_CMD} create-archive
	${RUN} ${MAKE_CMD} list-archive
	${RUN} ${MAKE_CMD} list-archive-wildcard
	${RUN} ${MAKE_CMD} depend-on-existing-member
	${RUN} ${MAKE_CMD} depend-on-nonexistent-member
	${RUN} ${MAKE_CMD} remove-archive

create-archive: ${ARCHIVE}
${ARCHIVE}: ${ARCHIVE}(${FILES})
	ar cru ${.TARGET} ${.OODATE}
	ranlib ${.TARGET}

list-archive: ${ARCHIVE}
	ar t ${.ALLSRC}

# XXX: I had expected that this dependency would select all *.mk files from
# the archive.  Instead, the globbing is done in the current directory.
# To prevent an overly long file list, the pattern is restricted to [at]*.mk.
list-archive-wildcard: ${ARCHIVE}([at]*.mk)
	${RUN} printf '%s\n' ${.ALLSRC:O:@member@${.TARGET:Q}': '${member:Q}@}

depend-on-existing-member: ${ARCHIVE}(archive.mk)
	${RUN} echo $@

depend-on-nonexistent-member: ${ARCHIVE}(nonexistent.mk)
	${RUN} echo $@

remove-archive:
	rm -f ${ARCHIVE}
