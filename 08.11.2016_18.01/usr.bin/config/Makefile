# $NetBSD: Makefile,v 1.2 2014/10/29 16:24:32 uebayasi Exp $

SUBDIR=			support

NOMAN=			# defined

.include <bsd.own.mk>

TESTSDIR=		${TESTSBASE}/usr.bin/config

TESTS_SH=		t_config

FILESDIR=		${TESTSDIR}
FILES=			d_deffs_redef
FILES+=			d_loop
FILES+=			d_loop2
FILES+=			d_min
FILES+=			d_no_pseudo
FILES+=			d_postponed_orphan
FILES+=			d_pseudo_parent
FILES+=			d_shadow_instance
FILES+=			support/arch/regress/conf/Makefile.regress
FILES+=			support/arch/regress/conf/files.regress
FILES+=			support/arch/regress/conf/std.regress
FILES+=			support/conf/files

.include <bsd.test.mk>
