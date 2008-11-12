#
# $P4: //depot/projects/trustedbsd/openbsm/bin/praudit/Makefile#4 $
#

CFLAGS+=	-I- -I ../.. -I ../../libbsm -L ../../libbsm -I.
PROG=		praudit
MAN=		praudit.1
DPADD=		/usr/lib/libbsm.a
LDADD=		-lbsm
BINDIR=		/usr/sbin

.include <bsd.prog.mk>
