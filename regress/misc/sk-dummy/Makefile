#	$OpenBSD: Makefile,v 1.3 2023/01/15 23:35:10 djm Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

PROG=	sk-dummy.so
NOMAN=

SSHREL=../../../../../usr.bin/ssh
.PATH: ${.CURDIR}/${SSHREL}

SRCS=sk-dummy.c
# From usr.bin/ssh
SRCS+=ed25519.c hash.c
OPENSSL?=	yes

CFLAGS+=	-fPIC

.if (${OPENSSL:L} == "yes")
CFLAGS+=	-DWITH_OPENSSL
.endif

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
CDIAGFLAGS+=	-Wno-unused-parameter
.if ${COMPILER_VERSION:L} != "gcc3"
CDIAGFLAGS+=	-Wold-style-definition
.endif

CFLAGS+=-I${.CURDIR}/${SSHREL}

.if (${OPENSSL:L} == "yes")
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
.endif

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $(OBJS) $(LDADD)

.include <bsd.prog.mk>

