# $FreeBSD$
# Makefile for file(1) cmd.
# Copyright (c) Ian F. Darwin 86/09/01 - see LEGAL.NOTICE.
#
# This software is not subject to any license of the American Telephone
# and Telegraph Company or of the Regents of the University of California.
#
# Permission is granted to anyone to use this software for any purpose on
# any computer system, and to alter it and redistribute it freely, subject
# to the following restrictions:
#
# 1. The author is not responsible for the consequences of use of this
#    software, no matter how awful, even if they arise from flaws in it.
# 2. The origin of this software must not be misrepresented, either by
#    explicit claim or by omission.  Since few users ever read sources,
#    credits must appear in the documentation.
# 3. Altered versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.  Since few users
#    ever read sources, credits must appear in the documentation.
# 4. This notice may not be removed or altered.

SRCDIR=	${.CURDIR}/../../contrib/file
.PATH: ${SRCDIR}

PROG=	file
MAN=	file.1 magic.5
SRCS=	file.c apprentice.c fsmagic.c softmagic.c ascmagic.c \
	compress.c is_tar.c  readelf.c print.c
#	compress.c is_tar.c  readelf.c internat.c print.c

MAGICPATH=	/usr/share/misc

CFLAGS+= -DMAGIC='"${MAGICPATH}/magic"' -DBUILTIN_ELF -DELFCORE -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I${SRCDIR}

LDADD+=	-lz
DPADD+=	${LIBZ}

CLEANFILES+=	magic magic.mgc magic.mime.mgc magic.mime.PITA

FILES=		magic magic.mgc ${SRCDIR}/magic.mime magic.mime.mgc
FILESDIR=	${MAGICPATH}

MAGFILES=	${SRCDIR}/Header\
		${SRCDIR}/Localstuff\
		${SRCDIR}/Magdir/[a-z]*

all: ${PROG} magic.mgc magic.mime.mgc

magic: ${MAGFILES}
	cat ${.ALLSRC} > ${.TARGET}

magic.mgc: mkmagic magic
	./mkmagic magic

magic.mime.mgc: mkmagic magic.mime
	ln -sf ${SRCDIR}/magic.mime magic.mime.PITA
	./mkmagic magic.mime.PITA
	mv magic.mime.PITA.mgc magic.mime.mgc

CLEANFILES+=	mkmagic
build-tools: mkmagic
mkmagic: apprentice.c print.c
	${CC} -DHAVE_CONFIG_H -DCOMPILE_ONLY \
	    -I${.CURDIR} -I${SRCDIR} -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
