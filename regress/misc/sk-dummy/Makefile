#	$OpenBSD: Makefile,v 1.7 2025/11/06 17:24:28 djm Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

PROG=	sk-dummy.so
NOMAN=

SSHREL=../../../../../usr.bin/ssh
.PATH: ${.CURDIR}/${SSHREL}

SRCS=sk-dummy.c fatal.c
# From usr.bin/ssh
SRCS+=ed25519-openssl.c
#SRCS+=ed25519.c
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
CDIAGFLAGS+=	-Wold-style-definition
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

CFLAGS+=-I${.CURDIR}/${SSHREL}

.if (${OPENSSL:L} == "yes")
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
.endif

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $(OBJS) $(LDADD)

.include <bsd.prog.mk>

