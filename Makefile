
PROG=		ctfdump
SRCS=		ctfdump.c elf.c

CFLAGS+=	-W -Wall -Wstrict-prototypes -Wno-unused -Wunused-variable

CFLAGS+=	-DZLIB
LDADD+=		-lz
DPADD+=		${LIBZ}

.include <bsd.prog.mk>
