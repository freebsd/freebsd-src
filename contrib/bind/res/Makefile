#
#	from @(#)Makefile	5.16 (Berkeley) 3/14/88
#	$Id: Makefile,v 8.13 1996/08/08 22:49:48 vixie Exp $
#

## ++Copyright++ 1988, 1995
## -
## Copyright (c) 1988, 1995
##    The Regents of the University of California.  All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
## 3. All advertising materials mentioning features or use of this software
##    must display the following acknowledgement:
## 	This product includes software developed by the University of
## 	California, Berkeley and its contributors.
## 4. Neither the name of the University nor the names of its contributors
##    may be used to endorse or promote products derived from this software
##    without specific prior written permission.
## 
## THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
## ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
## DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
## OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
## HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
## LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
## OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
## SUCH DAMAGE.
## -
## Portions Copyright (c) 1993 by Digital Equipment Corporation.
## 
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies, and that
## the name of Digital Equipment Corporation not be used in advertising or
## publicity pertaining to distribution of the document or software without
## specific, written prior permission.
## 
## THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
## WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
## CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.
## -
## --Copyright--

DESTDIR =
DESTLIB = /usr/lib
CC= cc
SHELL= /bin/sh
CDEBUG= -g
INCL = ../include
COMPINCL = ../compat/include
AR= ar cru
RANLIB= ranlib
DEFS=	
LOCDEFS= -DUSE_OPTIONS_H
INSTALL= install

AROBJS= ${ARPREF} ${OBJS} ${ARSUFF}

CFLAGS=	${CDEBUG} -I${INCL} -I${COMPINCL} ${DEFS} ${LOCDEFS}

SRCS=	base64.c herror.c res_debug.c res_data.c \
	res_comp.c res_init.c res_mkquery.c res_query.c res_send.c \
	getnetbyaddr.c getnetbyname.c getnetent.c getnetnamadr.c \
	gethnamaddr.c sethostent.c nsap_addr.c hostnamelen.c inet_addr.c \
	inet_ntop.c inet_neta.c inet_pton.c inet_net_ntop.c inet_net_pton.c

OBJS=	base64.o herror.o res_debug.o res_data.o \
	res_comp.o res_init.o res_mkquery.o res_query.o res_send.o \
	getnetbyaddr.o getnetbyname.o getnetent.o getnetnamadr.o \
	gethnamaddr.o sethostent.o nsap_addr.o hostnamelen.o inet_addr.o \
	inet_ntop.o inet_neta.o inet_pton.o inet_net_ntop.o inet_net_pton.o

all: libresolv.a

libresolv.a: ${OBJS}
	${AR} libresolv.a ${AROBJS}
	$(RANLIB) libresolv.a

install: ${DESTDIR}${DESTLIB}/libresolv.a

${DESTDIR}${DESTLIB}/libresolv.a: libresolv.a
	${INSTALL} -c -o bin -g bin -m 644 libresolv.a ${DESTDIR}${DESTLIB}/
	( cd ${DESTDIR}${DESTLIB} ; $(RANLIB) libresolv.a )

.c.o:
	${CC} ${CFLAGS} -c $*.c
	-${LDS} ld -x -r $*.o
	${LDS} mv a.out $*.o

clean: FRC
	rm -f errs a.out core libresolv.a tags .depend
	rm -f *.o *.BAK *.CKP *~ *.orig

depend: FRC
	mkdep ${CPPFLAGS} -I${INCL} -I${COMPINCL} ${DEFS} ${SRCS}

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
