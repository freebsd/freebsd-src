# $NetBSD: Makefile,v 1.6 2010/06/07 03:43:51 riz Exp $

TESTSDIR=	${TESTSBASE}/fs/tmpfs
WARNS=		4

TESTS_SH=	t_create
TESTS_SH+=	t_devices
TESTS_SH+=	t_dots
TESTS_SH+=	t_exec
TESTS_SH+=	t_link
TESTS_SH+=	t_mkdir
TESTS_SH+=	t_mknod
TESTS_SH+=	t_mount
TESTS_SH+=	t_pipes
TESTS_SH+=	t_read_write
TESTS_SH+=	t_readdir
TESTS_SH+=	t_remove
TESTS_SH+=	t_rename
TESTS_SH+=	t_rmdir
TESTS_SH+=	t_setattr
TESTS_SH+=	t_sizes
TESTS_SH+=	t_sockets
TESTS_SH+=	t_statvfs
TESTS_SH+=	t_symlink
TESTS_SH+=	t_times
TESTS_SH+=	t_trail_slash
TESTS_SH+=	t_truncate
TESTS_SH+=	t_vnd
TESTS_SH+=	t_vnode_leak

TESTS_C+=	t_renamerace

LDADD.t_renamerace+= -lrumpfs_tmpfs -lrumpvfs -lrump -lrumpuser -lpthread

FILES=		h_funcs.subr
FILESDIR=	${TESTSDIR}

PROGS=		h_tools
SRCS.h_tools=	h_tools.c
MAN.h_tools=	# empty
BINDIR.h_tools=	${TESTSDIR}

.include <bsd.test.mk>
