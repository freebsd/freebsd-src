# $NetBSD: Makefile,v 1.8 2014/08/25 20:40:53 joerg Exp $
#

NOMAN=		# defined

.include <bsd.own.mk>

.if ${MKPIC} != "no"

SUBDIR+=	helper_dso1 .WAIT helper_dso2 .WAIT \
		helper_ifunc_dso \
		helper_symver_dso0 .WAIT helper_symver_dso1 .WAIT \
		helper_symver_dso2 .WAIT \
		data

TESTSDIR=	${TESTSBASE}/libexec/ld.elf_so

TESTS_C+=	t_dlerror-cleared t_dlerror-false t_dlinfo t_dlvsym t_ifunc

LDADD.t_dlerror-false=	-Wl,-rpath,/var/nonexistent/lib
LDADD.t_dlvsym=		-Wl,-rpath,${TESTSDIR}/h_helper_symver_dso2
LDADD.t_ifunc=		-Wl,-rpath,${TESTSDIR} -lutil
DPADD.t_ifunc=		${LIBUTIL}

TESTS_SH+=		t_df_1_noopen t_dl_symver

BINDIR=			${TESTSDIR}
PROGS+=			h_df_1_noopen1
SRCS.h_df_1_noopen1=	h_df_1_noopen.c

PROGS+=			h_df_1_noopen2
SRCS.h_df_1_noopen2=	h_df_1_noopen.c
LDADD.h_df_1_noopen2=	-lpthread

PROGS+=			h_ifunc
SRCS.h_ifunc=		h_ifunc.c
IFUNCDIR!=		cd ${.CURDIR}/helper_ifunc_dso && ${PRINTOBJDIR}
LDADD.h_ifunc=		-Wl,-rpath,${TESTSDIR} -L${IFUNCDIR} -lh_helper_ifunc_dso

PROGS+=			h_locking
SRCS.h_locking=		h_locking.c
LDADD.h_locking=	-lpthread -Wl,--export-dynamic -Wl,-rpath,${TESTSDIR}

PROGS+=			h_dl_symver_v0
SRCS.h_dl_symver_v0=	h_dl_symver.c
V0ODIR!=		cd ${.CURDIR}/helper_symver_dso0 && ${PRINTOBJDIR}
LDADD.h_dl_symver_v0=	-L${V0ODIR} -lh_helper_symver_dso

PROGS+=			h_dl_symver_v1
SRCS.h_dl_symver_v1=	h_dl_symver.c
V1ODIR!=		cd ${.CURDIR}/helper_symver_dso1 && ${PRINTOBJDIR}
LDADD.h_dl_symver_v1=	-L${V1ODIR} -lh_helper_symver_dso

PROGS+=			h_dl_symver_v2
SRCS.h_dl_symver_v2=	h_dl_symver.c
V2ODIR!=		cd ${.CURDIR}/helper_symver_dso2 && ${PRINTOBJDIR}
LDADD.h_dl_symver_v2=	-L${V2ODIR} -lh_helper_symver_dso

.include <bsd.test.mk>

.else

NOPROG=	# defined
.include <bsd.prog.mk>

.endif
