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

# $Id: Makefile,v 8.20 2000/07/11 06:41:40 vixie Exp $

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
# -D__BIND_NOSTATIC -Wimplicit
LD_LIBFLAGS= -x -r
AR= ar cru
RANLIB= ranlib
INSTALL= install
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin
THREADED= threaded

SRCS=	dns.c dns_gr.c dns_ho.c dns_nw.c dns_pr.c dns_pw.c \
	dns_sv.c gai_strerror.c gen.c gen_gr.c gen_ho.c \
	gen_ng.c gen_nw.c gen_pr.c gen_pw.c gen_sv.c \
	getaddrinfo.c getgrent.c getgrent_r.c gethostent.c \
	gethostent_r.c getnameinfo.c getnetent.c getnetent_r.c \
	getnetgrent.c getnetgrent_r.c getprotoent.c \
	getprotoent_r.c getpwent.c getpwent_r.c getservent.c \
	getservent_r.c hesiod.c irs_data.c \
	irp.c irp_gr.c irp_ho.c irp_ng.c irp_nw.c \
	irp_pr.c irp_pw.c irp_sv.c irpmarshall.c \
	lcl.c lcl_gr.c \
	lcl_ho.c lcl_ng.c lcl_nw.c lcl_pr.c lcl_pw.c \
	lcl_sv.c nis.c nis_gr.c nis_ho.c nis_ng.c nis_nw.c \
	nis_pr.c nis_pw.c nis_sv.c nul_ng.c util.c

OBJS=	dns.${O} dns_gr.${O} dns_ho.${O} dns_nw.${O} dns_pr.${O} dns_pw.${O} \
	dns_sv.${O} gai_strerror.${O} gen.${O} gen_gr.${O} gen_ho.${O} \
	gen_ng.${O} gen_nw.${O} gen_pr.${O} gen_pw.${O} gen_sv.${O} \
	getaddrinfo.${O} getgrent.${O} getgrent_r.${O} gethostent.${O} \
	gethostent_r.${O} getnameinfo.${O} getnetent.${O} getnetent_r.${O} \
	getnetgrent.${O} getnetgrent_r.${O} getprotoent.${O} \
	getprotoent_r.${O} getpwent.${O} getpwent_r.${O} getservent.${O} \
	getservent_r.${O} hesiod.${O} irs_data.${O} \
	irp.${O} irp_gr.${O} irp_ho.${O} irp_ng.${O} irp_nw.${O} \
	irp_pr.${O} irp_pw.${O} irp_sv.${O} irpmarshall.${O} \
	lcl.${O} lcl_gr.${O} \
	lcl_ho.${O} lcl_ng.${O} lcl_nw.${O} lcl_pr.${O} lcl_pw.${O} \
	lcl_sv.${O} nis.${O} nis_gr.${O} nis_ho.${O} nis_ng.${O} nis_nw.${O} \
	nis_pr.${O} nis_pw.${O} nis_sv.${O} nul_ng.${O} util.${O}

all: ${LIBBIND}

${LIBBIND}: ${OBJS}
	-( cd ${THREADED} ; \
	${AR} ${LIBBINDR} ${ARPREF} ${OBJS} ${ARSUFF} ; \
	${RANLIB} ${LIBBINDR} )
	${AR} ${LIBBIND} ${ARPREF} ${OBJS} ${ARSUFF}
	${RANLIB} ${LIBBIND}

.c.${O}:
	if test ! -d ${THREADED} ; then mkdir ${THREADED} ; else true ; fi
	-(${CC} ${CPPFLAGS} ${CFLAGS} ${BOUNDS} ${REENTRANT} -c $*.c \
	 -o ${THREADED}/$*.${O} ; \
	 ${LDS} ${LD} ${LD_LIBFLAGS} ${THREADED}/$*.${O} \
	 -o ${THREADED}/$*.out && \
	 ${LDS} mv ${THREADED}/$*.out ${THREADED}/$*.${O})
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

testirpd: testirpd.o ${LIBBIND}
	${CC} ${CDEBUG} ${LDFLAGS} -o testirpd testirpd.o ${LIBBIND} ${SYSLIBS}

	
install:

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
