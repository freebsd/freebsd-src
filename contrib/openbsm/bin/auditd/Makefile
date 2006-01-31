#
# $P4: //depot/projects/trustedbsd/openbsm/bin/auditd/Makefile#2 $
#

CFLAGS+=	-I- -I ../.. -I ../../libbsm -L ../../libbsm -I.
PROG=		auditd
SRCS=		audit_warn.c auditd.c
MAN=		auditd.8
DPADD=		/usr/lib/libbsm.a
LDADD=		-lbsm
BINDIR=		/usr/sbin

.include <bsd.prog.mk>
