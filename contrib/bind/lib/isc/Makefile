# Copyright (c) 1996 by Internet Software Consortium
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
# ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
# CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
# ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
# SOFTWARE.

# $Id: Makefile,v 8.15 1997/12/03 22:36:08 halley Exp $

# these are only appropriate for BSD 4.4 or derivatives, and are used in
# development.  normal builds will be done in the top level directory and
# this Makefile will be invoked with a lot of overrides for the following:
SYSTYPE= bsdos
DESTDIR =
DESTLIB = /usr/local/lib
O=o
A=a
CC= cc
LD= ld
SHELL= /bin/sh
CDEBUG= -g
TOP= ../..
INCL = ${TOP}/include
PORTINCL = ${TOP}/port/${SYSTYPE}/include
LIBBIND = ${TOP}/lib/libbind.${A}
CFLAGS=	${CDEBUG} -I${PORTINCL} -I${INCL}
LD_LIBFLAGS= -x -r
AR= ar cruv
RANLIB= ranlib
INSTALL= install

SRCS=	tree.c base64.c bitncmp.c assertions.c \
	memcluster.c logging.c heap.c \
	eventlib.c ev_connects.c ev_files.c \
	ev_timers.c ev_streams.c ev_waits.c

OBJS=	tree.${O} base64.${O} bitncmp.${O} assertions.${O} \
	memcluster.${O} logging.${O} heap.${O} \
	eventlib.${O} ev_connects.${O} ev_files.${O} \
	ev_timers.${O} ev_streams.${O} ev_waits.${O}

all: ${LIBBIND}

${LIBBIND}: ${OBJS}
	${AR} ${LIBBIND} ${ARPREF} ${OBJS} ${ARSUFF}
	${RANLIB} ${LIBBIND}

.c.${O}:
	${CC} ${CPPFLAGS} ${CFLAGS} -c $*.c
	-${LDS} ${LD} ${LD_LIBFLAGS} $*.${O} && ${LDS} mv a.out $*.${O}

distclean: clean

clean: FRC
	rm -f .depend a.out core ${LIB} tags
	rm -f *.${O} *.BAK *.CKP *~

depend: FRC
	mkdep -I${INCL} -I${PORTINCL} ${CPPFLAGS} ${SRCS}

links: FRC
	@set -e; ln -s SRC/*.[ch] .

install:

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
