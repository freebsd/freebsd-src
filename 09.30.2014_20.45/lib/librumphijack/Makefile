#	$NetBSD: Makefile,v 1.8 2012/12/02 17:36:10 pgoyette Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/librumphijack

TESTS_SH=	t_asyncio
TESTS_SH+=	t_config
TESTS_SH+=	t_cwd
TESTS_SH+=	t_sh
TESTS_SH+=	t_tcpip
TESTS_SH+=	t_vfs

BINDIR=		${TESTSDIR}
MKMAN=		no

PROGS+=		h_client
PROGS+=		h_netget
PROGS+=		h_cwd

FILES=		netstat.expout index.html
FILESDIR=	${TESTSDIR}

# ssh test
FILES+=		ssh_config.in
FILES+=		ssh_host_key
FILES+=		ssh_host_key.pub
FILES+=		sshd_config.in

.include <bsd.test.mk>
