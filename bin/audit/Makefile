# 
# $P4: //depot/projects/trustedbsd/openbsm/bin/audit/Makefile#2 $
#

CFLAGS+=	-I- -I ../.. -I ../../libbsm -L ../../libbsm -I.
PROG=		audit
MAN=		audit.8
DPADD=		/usr/lib/libbsm.a
LDADD=		-lbsm
BINDIR=		/usr/sbin

.include <bsd.prog.mk>
