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
#	$Id: Makefile,v 1.14 1996/04/23 05:18:42 pst Exp $
#

PROG=	telnet

SRCS=	authenc.c commands.c main.c network.c ring.c sys_bsd.c telnet.c \
	terminal.c tn3270.c utilities.c

CFLAGS+=-DTERMCAP -DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DENCRYPTION
CFLAGS+=-I${.CURDIR}/../../lib
LDADD+= -L${TELNETOBJDIR} -ltermcap -ltelnet
DPADD+= ${TELNETOBJDIR}/libtelnet.a ${LIBTERMCAP}

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_EBONES))
CFLAGS+=-DAUTHENTICATION -DKRB4
LDADD+= -ldes -lkrb
DPADD+= ${LIBDES} ${LIBKRB}
.endif

# Used only in krb4encpwd.c and rsaencpwd.c (libtelnet), not yet active
#LDADD+= -ldescrypt

.include <bsd.prog.mk>
