#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$
#

PROG=	telnet

CFLAGS+=-DKLUDGELINEMODE -DUSE_TERMIO #-DAUTHENTICATION -DENCRYPTION
CFLAGS+=-DENV_HACK
CFLAGS+=-DSKEY
CFLAGS+=-I${.CURDIR}/../../lib

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

DPADD=	${LIBTERMCAP} ${LIBTELNET}
LDADD=	-ltermcap ${LIBTELNET}

.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DINET6
CFLAGS+=-DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

SRCS=	authenc.c commands.c main.c network.c ring.c sys_bsd.c telnet.c \
	terminal.c tn3270.c utilities.c

# These are the sources that have encryption stuff in them.
CRYPT_SRC= authenc.c commands.c externs.h main.c network.c
CRYPT_SRC+= ring.c ring.h telnet.c terminal.c utilities.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

.include <bsd.prog.mk>

nocrypt:
	@echo "Encryption code already removed."
