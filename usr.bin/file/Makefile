# Makefile for file(1) cmd.
# Copyright (c) Ian F. Darwin 86/09/01 - see LEGAL.NOTICE.
# @(#)$Id: Makefile,v 1.5 1996/08/17 22:27:08 wosch Exp $
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
#
# 2. The origin of this software must not be misrepresented, either by
#    explicit claim or by omission.  Since few users ever read sources,
#    credits must appear in the documentation.
#
# 3. Altered versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.  Since few users
#    ever read sources, credits must appear in the documentation.
#
# 4. This notice may not be removed or altered.
#
# Hacked and dismembered for bmake (Geoff Rehmet).

MAGICDIR=	/usr/share/misc
MAGICOWN=	bin
MAGICGRP=	bin
MAGICMODE=	444

CFLAGS+=	-DMAGIC='"$(MAGICDIR)/magic"'

PROG=		file
SRCS=		file.c apprentice.c fsmagic.c softmagic.c ascmagic.c \
		compress.c is_tar.c  print.c international.c

MAN1=		file.1
MAN5=		magic.5

CLEANFILES+=	magic

MAGFILES=	$(.CURDIR)/Magdir/Header\
		$(.CURDIR)/Magdir/Localstuff\
		$(.CURDIR)/Magdir/[a-z]*

all:	file magic

magic:	$(MAGFILES)
	cat $(MAGFILES) > $(.TARGET)

beforeinstall:
	$(INSTALL) $(COPY) -o $(MAGICOWN) -g $(MAGICGRP) -m $(MAGICMODE) \
	    magic $(DESTDIR)$(MAGICDIR)/magic

etc-magic:
	rm -f ${DESTDIR}/etc/magic
	ln -s ${MAGICDIR}/magic ${DESTDIR}/etc/magic

.include <bsd.prog.mk>
