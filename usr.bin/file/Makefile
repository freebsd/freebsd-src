MAGIC=		/etc/magic
MAGICOWN=	bin
MAGICGRP=	bin
MAGICMODE=	444

PROG=		file
SRCS=		file.c apprentice.c fsmagic.c softmagic.c ascmagic.c is_tar.c \
		print.c compress.c
CFLAGS+=	-DMAGIC='"$(MAGIC)"'
MAN1=		file.1
MAN5=		magic.5

CLEANFILES+=	magic
all:		file magic

MAGFILES=	$(.CURDIR)/magdir/Header\
		$(.CURDIR)/magdir/Localstuff\
		$(.CURDIR)/magdir/[a-z]*
magic:		$(MAGFILES)
	cat $(MAGFILES) > $(.TARGET)

afterinstall:
	install -c -o $(MAGICOWN) -g $(MAGICGRP) -m $(MAGICMODE) magic \
		$(DESTDIR)$(MAGIC)

.include <bsd.prog.mk>
