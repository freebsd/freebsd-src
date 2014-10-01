# $NetBSD: Makefile,v 1.1 2012/03/17 16:33:11 jruoho Exp $

NOMAN=  	# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/bin/df

TESTS_SH=	t_df

BINDIR=		${TESTSDIR}
PROG=		h_df
.PATH:  	${NETBSDSRCDIR}/bin/df
SRCS=		df.c getmntinfo.c

LDADD+=		-lutil
DPADD+=		${LIBUTIL}

# Pass -DINTREE to make to test using humanize_number.c in source tree
# directly instead of the one in libc.
.if defined(INTREE)
.PATH:		${NETBSDSRCDIR}/lib/libc/gen
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libc/include
SRCS+=		humanize_number.c
.endif

.include <bsd.test.mk>
