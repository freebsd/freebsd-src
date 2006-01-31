#
# $P4: //depot/projects/trustedbsd/openbsm/tools/Makefile#3 $
#

CFLAGS+=	-I- -I .. -I ../libbsm -L ../libbsm -I.
PROG=		audump
NO_MAN=
DPADD=		/usr/lib/libbsm.a
LDADD=		-lbsm
BINDIR=		/usr/sbin
WARNS=		3

.include <bsd.prog.mk>
