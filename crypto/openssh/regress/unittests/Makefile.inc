#	$OpenBSD: Makefile.inc,v 1.9 2016/11/01 13:43:27 tb Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

# enable warnings
WARNINGS=Yes

DEBUG=-g
CFLAGS+=	-fstack-protector-all
CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wchar-subscripts
CDIAGFLAGS+=	-Wcomment
CDIAGFLAGS+=	-Wformat
CDIAGFLAGS+=	-Wformat-security
CDIAGFLAGS+=	-Wimplicit
CDIAGFLAGS+=	-Winline
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wparentheses
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wreturn-type
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wstrict-aliasing
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wswitch
CDIAGFLAGS+=	-Wtrigraphs
CDIAGFLAGS+=	-Wuninitialized
CDIAGFLAGS+=	-Wunused
.if ${COMPILER_VERSION} == "gcc4"
CDIAGFLAGS+=	-Wpointer-sign
CDIAGFLAGS+=	-Wold-style-definition
.endif

SSHREL=../../../../../usr.bin/ssh

CFLAGS+=-I${.CURDIR}/../test_helper -I${.CURDIR}/${SSHREL}

.if exists(${.CURDIR}/../test_helper/${__objdir})
LDADD+=-L${.CURDIR}/../test_helper/${__objdir} -ltest_helper
DPADD+=${.CURDIR}/../test_helper/${__objdir}/libtest_helper.a
.else
LDADD+=-L${.CURDIR}/../test_helper -ltest_helper
DPADD+=${.CURDIR}/../test_helper/libtest_helper.a
.endif

.if exists(${.CURDIR}/${SSHREL}/lib/${__objdir})
LDADD+=-L${.CURDIR}/${SSHREL}/lib/${__objdir} -lssh
LIBSSH=${.CURDIR}/${SSHREL}/lib/${__objdir}/libssh.a
.else
LDADD+=-L${.CURDIR}/${SSHREL}/lib -lssh
LIBSSH=${.CURDIR}/${SSHREL}/lib/libssh.a
.endif
DPADD+=${LIBSSH}
${PROG}: ${LIBSSH}
${LIBSSH}:
	cd ${.CURDIR}/${SSHREL} && ${MAKE} lib

LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
