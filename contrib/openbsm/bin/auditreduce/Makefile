#
# $P4: //depot/projects/trustedbsd/openbsm/bin/auditreduce/Makefile#4 $
#

CFLAGS+=	-I- -I ../.. -I ../../libbsm -L ../../libbsm -I.
PROG=		auditreduce
MAN=		auditreduce.1
DPADD=		/usr/lib/libbsm.a
LDADD=		-lbsm
BINDIR=		/usr/sbin

.include <bsd.prog.mk>
