# $NetBSD: Makefile,v 1.6 2014/10/18 02:22:35 joerg Exp $

WARNS=0
NOMAN=1

.include <bsd.own.mk>

PROG=	all_sync_ops_linkable

.if "${ACTIVE_CC}" == "clang" || \
    ("${ACTIVE_CC}" == "gcc" && "${HAVE_GCC}" == "48")

CXXFLAGS+= -std=c++11
PROG_CXX+=	cpp_atomic_ops_linkable

.endif

proginstall:
	@echo This directory features link time only tests.

.include <bsd.prog.mk>

