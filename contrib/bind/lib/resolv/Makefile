# Copyright (c) 1996,1999 by Internet Software Consortium
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

# $Id: Makefile,v 8.34 2000/07/11 06:41:42 vixie Exp $

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
LIBBINDR = ../${TOP}/lib/libbind_r.${A}
CFLAGS=	${CDEBUG}
CPPFLAGS= -I${PORTINCL} -I${INCL}
# -D__BIND_NOSTATIC
LD_LIBFLAGS= -x -r
AR= ar cru
RANLIB= ranlib
INSTALL= install
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin
THREADED= threaded

SRCS=	herror.c res_debug.c res_data.c res_comp.c res_init.c \
	res_mkquery.c res_query.c res_send.c res_sendsigned.c \
	res_mkupdate.c res_update.c \
	res_findzonecut.c

OBJS=	herror.${O} res_debug.${O} res_data.${O} res_comp.${O} res_init.${O} \
	res_mkquery.${O} res_query.${O} res_send.${O} res_sendsigned.${O} \
	res_mkupdate.${O} res_update.${O} \
	res_findzonecut.${O}

all: ${LIBBIND}

${LIBBIND}: ${OBJS}
	( cd ${THREADED} ; \
	${AR} ${LIBBINDR} ${ARPREF} ${OBJS} ${ARSUFF} ; \
	${RANLIB} ${LIBBINDR} )
	${AR} ${LIBBIND} ${ARPREF} ${OBJS} ${ARSUFF}
	${RANLIB} ${LIBBIND}

.c.${O}:
	if test ! -d ${THREADED} ; then mkdir ${THREADED} ; else true ; fi
	${CC} ${CPPFLAGS} ${CFLAGS} ${BOUNDS} ${REENTRANT} -c $*.c \
	 -o ${THREADED}/$*.${O}
	-${LDS} ${LD} ${LD_LIBFLAGS} ${THREADED}/$*.${O} \
	 -o ${THREADED}/$*.out && \
	 ${LDS} mv ${THREADED}/$*.out ${THREADED}/$*.${O}
	${CC} ${CPPFLAGS} ${CFLAGS} ${BOUNDS} -c $*.c
	-${LDS} ${LD} ${LD_LIBFLAGS} $*.${O} -o $*.out && \
	 ${LDS} mv $*.out $*.${O}

distclean: clean

clean: FRC
	rm -f .depend a.out core ${LIB} tags
	rm -f *.${O} *.BAK *.CKP *~
	rm -f ${THREADED}/*.${O}
	-if test -d ${THREADED} ; then rmdir ${THREADED}; else true; fi

depend: FRC
	mkdep -I${INCL} -I${PORTINCL} ${CPPFLAGS} ${SRCS}

links: FRC
	@set -e; ln -s SRC/*.[ch] .

install:

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
