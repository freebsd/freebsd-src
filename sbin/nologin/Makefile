#	@(#)Makefile	8.2 (Berkeley) 4/22/94
# $FreeBSD: src/sbin/nologin/Makefile,v 1.9 2003/11/17 06:39:38 das Exp $

PROG=	nologin
MAN=	nologin.5 nologin.8

# It is important that nologin be statically linked for security
# reasons.  A dynamic non-setuid binary can be linked against a trojan
# libc by setting LD_LIBRARY_PATH appropriately.  Both sshd(8) and
# login(1) make it possible to log in with an unsanitized environment,
# rendering a dynamic nologin binary virtually useless.
NOSHARED=	YES

.include <bsd.prog.mk>
