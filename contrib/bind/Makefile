# Makefile for BIND>=4.9 top level
# vixie@decwrl December, 1992 [original]
#
# $Id: Makefile,v 8.49 1997/06/01 20:34:25 vixie Exp vixie $

## Copyright (c) 1989
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
##
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

VER = 4.9.6-REL
SHELL = /bin/sh
MAKE = make
DESTDIR =

INCL = include
RES = res/libresolv.a
COMPLIB = compat/lib/lib44bsd.a

# The default build parameters are given for 4.4 BSD.  They should
# be overridden for other operating systems as indicated below.

#(BSD4.4, BSD/OS, FreeBSD, NetBSD, other net2-alikes)
#
# DO NOT COMMENT THESE OUT.  OVERRIDE THEM BELOW.
#
CC = cc $(CPPFLAGS)
LDFLAGS =
CDEBUG = -O -g
CPPFLAGS=
INDOT =
XFER_INDOT =
LEX = lex -I
LIBS = -ll -lutil
PIDDIR = /var/run
DESTBIN = /usr/bin
DESTSBIN = /usr/sbin
DESTEXEC = /usr/libexec
DESTMAN = /usr/share/man
DESTHELP = /usr/share/misc
DESTLIB = /usr/lib
DESTINC = /usr/include
COMPINCL = compat/include
LDS = 
CATEXT = 0
INSTALL_COMPAT = install
INSTALL = install
RANLIB = ranlib
AR = ar cru
MANROFF = ( tbl | nroff -man )
MANDIR = cat
PS = ps
IOT = ABRT
SHCC =
SHLD =
PIC =

# (NetBSD - for details on shared library building, see shres/netbsd/INSTALL)
#uncomment next three lines to build a shared library version of libresolv
#SHRES = shres/netbsd
#SHCC = cc $(CPPFLAGS)
#PIC = -fpic

#(Linux - on modern systems, all you need to do is rename or remove
# compat/include/sys/cdefs.h.  See doc/info/Linux for more information.)
#CC = gcc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSYSV
#LEX=flex -8 -I
#INSTALL_COMPAT = install-compat
#LIBS = -lfl
#DESTEXEC = /usr/sbin
#DESTMAN = /usr/man
#MANDIR = man
#MANROFF = cat
#DESTHELP = /usr/lib
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT
#uncomment next line to build a shared library version of libresolv
#SHRES = shres/linux
#uncomment next line to build tools and named with shared libresolv
#RES = $(SHRES)/libresolv.so
# ... and then (for shared) uncomment these lines too:
#SHCC = gcc $(CPPFLAGS) -fomit-frame-pointer -pipe
#PIC= -fpic
#SHMAJVER=4
#SHVER=$(SHMAJVER).9.5.0.4
#SHLD= gcc -shared -Wl,-rpath,$(DESTDIR)$(DESTLIB),-soname,libresolv.so.$(SHMAJVER) -lfl

#(CRAY)
#CDEBUG = -g
#LEX=lex
#IOT = IOT

#(DEC AXP OSF/1 Version 2.1 and earlier)
#CC = cc $(CPPFLAGS)
#CDEBUG = -g3 -O2 -Olimit 2000
#CPPFLAGS =
#LEX = lex
#LIBS = -ll -lutil
#DESTEXEC = /usr/sbin
#COMPINCL = compat/include
#INSTALL_COMPAT = install-compat
#INSTALL = installbsd
#MANDIR = man
#MANROFF = cat
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT

#(DEC AXP OSF/1 Version 3.0 and after)
#CC = cc $(CPPFLAGS) -std
#CDEBUG = -g3 -O2 -Olimit 2000
#CPPFLAGS = -DCHECK_UDP_SUM
#LEX = lex
#LIBS = -ll -lutil -lmld
#DESTEXEC = /usr/sbin
#COMPINCL = compat/include
#INSTALL_COMPAT = install-compat
#INSTALL = installbsd
#MANDIR = man
#MANROFF = cat
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT
#KSYMS = /vmunix
#KMEM = /dev/kmem
#UDPSUM = udpcksum

#(irix4)
#CC = cc $(CPPFLAGS) -xansi -signed
#CDEBUG = -g
#CPPFLAGS = -D__STDC__ -D_BSD_SIGNALS -DSIG_FN=int -DCHECK_UDP_SUM -DKMAP
#LEX = lex
#LIBS = -ll -lmld
#DESTSBIN = /usr/etc
#DESTEXEC = /usr/etc
#DESTMAN = /usr/catman/local
#DESTHELP = /usr/lib
#INSTALL = bsdinstall.sh
#RANLIB = :
#PS = ps -p
#(bsdinstall.sh is in conf/)
#IOT = IOT
#KSYMS = /unix
#KMEM = /dev/kmem
#UDPSUM = udpcksum

#(irix5 and irix6.2)
#CC = cc $(CPPFLAGS) -xansi -signed
#CDEBUG = -g
#CPPFLAGS = -D__STDC__ -D_BSD_SIGNALS -DSIG_FN=void -D__BIT_TYPES_DEFINED__ \
#		-DCHECK_UDP_SUM -DKMAP
#LEX = lex
#LIBS = -ll -lelf
#DESTSBIN = /usr/etc
#DESTEXEC = /usr/etc
#DESTMAN = /usr/share/catman/local
#DESTHELP = /usr/lib
#INSTALL = bsdinstall.sh
#RANLIB = :
#PS = ps -p
#(bsdinstall.sh is in conf/)
#IOT = IOT
#KSYMS = /unix
#KMEM = /dev/kmem
#UDPSUM = udpcksum
# shared libresolv.o options below.
# they could work under irix6, if the cmplr didn't change alot
#SHRES = shres/irix
#SHCC = $(CC) $(CPPFLAGS)
#SHLD = ld -shared
#RES = $(SHRES)/libresolv.so

#(sunos4.x)
#CC = /usr/bin/cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSUNOS4 -DSPRINTF_CHAR -DCHECK_UDP_SUM -Dconst=
#INDOT = in.
#XFER_INDOT = in.
#LEX = lex
#LIBS = -ll
#(add "-ldl" to LIBS if your links fail with "dlopen" problems.  this indicates
# that your libc.so file is screwed up, but it's easier to fix this than that.)
#PIDDIR = /etc
#DESTSBIN = /usr/etc
#DESTEXEC = /usr/etc
#DESTHELP = /usr/lib
#COMPINCL = compat/include
#MANDIR = man
#CATEXT = $$$$N
#MANROFF = cat
#INSTALL_COMPAT = install-compat
#INSTALL = install
#IOT = IOT
#KSYMS = /vmunix
#KMEM = /dev/kmem
#UDPSUM = _udp_cksum
# (for details on shared library building, see shres/sunos/INSTALL;
#  note that shres/solaris/INSTALL has some good testing notes for sunos)
#uncomment next line to build a shared library version of libresolv
#SHRES = shres/sunos
# uncomment only one of the pairs of lines below
# -- if you use gcc (recommended)
#    (if -fpic doesn't work, see shres/sunos/PROBLEMS)
#SHCC = gcc -DSUNSECURITY $(CPPFLAGS)
#PIC= -B/usr/bin/ -fpic -D_res=_res_shlib
# -- if you use Sun's cc
#SHCC = /usr/bin/cc -DSUNSECURITY $(CPPFLAGS)
#PIC = -pic -D_res=_res_shlib

#(ULTRIX/RISC - also see below)
#CC = cc $(CPPFLAGS) -Olimit 2000 -YPOSIX

#(ULTRIX, other 4.[23]bsd-alikes)
#CDEBUG = -g
#CPPFLAGS = -DCHECK_UDP_SUM
#LEX = lex
#LIBS = -ll
#PIDDIR = /etc
#DESTSBIN = /usr/etc
#DESTEXEC = /usr/etc
#DESTHELP = /usr/lib
#COMPINCL = compat/include
#CATEXT = $$$$N
#INSTALL_COMPAT = install-compat
#INSTALL = install
#DESTMAN = /usr/man
#MANDIR = man
#MANROFF = cat
#IOT = IOT
#KSYMS = /vmunix
#KMEM = /dev/kmem
#UDPSUM = udpcksum

#(solaris2.x/sunos5.x)
#be careful with installation - note in particular that the system ships
#with /usr/sbin/in.named (with in.) and /usr/sbin/named-xfer (without in.)
#to emulate this naming, INDOT should be set to 'in.' while XFER_INDOT 
#should be empty. also, under solaris2.x, use 'make install' at your own risk.
#don't include sys/stream.h via netinet/in.h by defining _SYS_STREAM_H.
#->and note that sun moved their nslookup to /usr/sbin for some odd reason;
#  when you install this one in /usr/bin you should delete sun's or else you
#  will get confusing results since you never know which nslookup you're using.
#PIDDIR = /etc
#LEX = lex
#uncomment next line for Sun C compiler
#CC = /opt/SUNWspro/bin/cc $(CPPFLAGS) -R$(DESTLIB)
#uncomment next two lines for GNU C compiler
#CC = gcc $(CPPFLAGS)
#LDFLAGS=-Xlinker -R$(DESTLIB)
#otherwise uncomment the next line and hope for the best
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPPFLAGS = -DSVR4 -D_SYS_STREAM_H -DMAXHOSTNAMELEN=256
#CPPFLAGS = $(CPPPFLAGS) $(SOLCOMPAT)
#INDOT = in.
#XFER_INDOT =
#LIBS = -ll -lnsl -lsocket
#DESTEXEC = /usr/sbin
#DESTHELP = /usr/lib
#INSTALL_COMPAT = install-compat
#LDS = :
#INSTALL = /usr/ucb/install
#RANLIB = :
#ARPREF = `lorder
#ARSUFF = | tsort`
#MANDIR = man
#MANROFF = cat
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT
#uncomment next line for link-compatibility with Solaris resolver library
#SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
#	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
#	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
#	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
#	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
#	-Dgetnetent=res_getnetent
# for details on shared library building, see shres/solaris/INSTALL
#uncomment next line to build a shared library version of libresolv
#SHRES = shres/solaris
#uncomment next line to build tools and named with shared libresolv
#if you do so, you *must* define SOLCOMPAT as well (and it must be in CPPFLAGS)
#RES = $(SHRES)/libresolv.so
# uncomment only one of the triplets of lines below
# -- if you use gcc (recommended)
#    (if -fpic doesn't work, see shres/solaris/ISSUES)
#SHCC = gcc -B/usr/ccs/bin/ $(CPPPFLAGS)
#PIC= -fpic
#SHLD = /usr/ccs/bin/ld -G
# -- if you use Sun's cc
#SHCC = /opt/SUNWspro/bin/cc $(CPPPFLAGS)
#PIC = -Kpic
#SHLD = /opt/SUNWspro/bin/cc -G

#(hpux7.0,hpux8.0,hpux9.0,)
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DCHECK_UDP_SUM
#LEX = lex
#LIBS = -ll -lBSD
#PIDDIR = /etc
#DESTSBIN = /etc
#DESTEXEC = /etc
#DESTMAN = /usr/man
#DESTHELP = /usr/lib
#INSTALL = bsdinstall.sh
#RANLIB = :
#MANROFF = cat
#PS = ps -p
#KSYMS = /hp-ux
#KMEM = /dev/kmem
#UDPSUM = udpcksum
#(bsdinstall.sh is in conf/)

#(hpux10.10)
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -Dconst=
#LEX = lex
#LIBS = -ll
#PIDDIR = /etc
#DESTSBIN = /usr/sbin
#DESTEXEC = /usr/sbin
#DESTMAN = /usr/man
#DESTHELP = /usr/lib
#LDS = :
#INSTALL = bsdinstall.sh
#RANLIB = :
#MANROFF = cat
#PS = ps -p
#INSTALL_COMPAT = install-install
#(bsdinstall.sh is in conf/)

#(apollo domainos)
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -U_APOLLO_SOURCE -D_BSD_SOURCE -DSPRINTF_CHAR -D_CLASSIC_BSD_SPRINTF
#LIBS = -ll 
#DESTSBIN = /etc
#DESTEXEC = /etc
#DESTHELP = /usr/lib
#DESTMAN = /usr/man
#PIDDIR = /etc
#LEX = lex
#PS = /bsd4.3/bin/ps
#IOT = IOT

#(AIX3)
#CC = bsdcc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DBSD=43
#LIBS = -ll
#DESTEXEC = /usr/sbin
#INSTALL = /usr/ucb/install
#CATEXT = $$$$N
#LEX = lex
#PS = ps -p
#IOT = IOT

#(AIX4)
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -D_BSD=44
#LIBS = -ll -lbsd
#DESTEXEC = /usr/sbin
#INSTALL = /usr/ucb/install
#CATEXT = $$$$N
#LEX = lex
#PS = ps -p
#IOT = IOT

# (ConvexOS-10.x)
#CC = gcc $(CPPFLAGS) -g -O2 -fpcc-struct-return -fno-builtin -funsigned-char
#CDEBUG = -g
#CPPFLAGS =
#INDOT =
#LEX = flex -8 -I
#LIBS = -lfl 
#PIDDIR = /usr/local/etc
#DESTBIN = /usr/local/bin
#DESTSBIN = /usr/local/bin
#DESTEXEC = /usr/local/etc
#DESTMAN = /usr/local/man
#DESTHELP = /usr/local/etc
#DESTLIB = /usr/local/lib
#DESTINC = /usr/local/include
#COMPINCL = compat/include
#LDS = 
#INSTALL_COMPAT = install
#INSTALL = install
#RANLIB = ranlib
#IOT = IOT

#(NEC EWS4800 EWS-UX/V Rel4.0/Rel4.2,UX/4800)
#be careful with installation - note in particular that the system ships
#with /usr/sbin/in.named (with in.) and /usr/sbin/named-xfer (without in.)
#to emulate this naming, INDOT should be set to in. and XFER_INDOT should be
#set to empty. also, under EWS4800, use 'make install' at your own risk. 
#CC = /usr/necccs/bin/cc $(CPPFLAGS)
#CC = /usr/abiccs/bin/cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSVR4 -DUSE_POSIX
#INDOT = in.
#XFER_INDOT =
#LEX = lex
#LIBS = -ll -lnsl -lsocket
#DESTBIN = /usr/local/bin
#DESTSBIN = /etc
#DESTEXEC = /etc
#DESTHELP = /usr/local/lib
#DESTLIB = /usr/local/lib
#DESTINC = /usr/local/include
#DESTMAN = /usr/local/man
#PIDDIR = /etc
#LDS = :
#RANLIB = :
#COMPINCL = compat/include
#CATEXT = $$$$N
#INSTALL_COMPAT = install-compat
#INSTALL = /usr/ucb/install
#PS = ps -p
#IOT = IOT

# SCO OSE5, SCO ODT 3.0, SCO Unix 3.2v4.2
# uncomment only one set of the CC/CDEBUG lines below
# -- OSE5 using SCO's cc
#CC = cc $(CPPFLAGS)
#CDEBUG=-O3
# -- ODT 3.0 / 3.2v4.2 using SCO's cc ( lots of warnings during compilation )
#CC = cc $(CPPFLAGS)
#CDEBUG=-O
# -- if you use gcc ( gcc highly recommended on 3.2v4.2 / ODT 3.0 )
#CC = gcc $(CPPFLAGS)
#CDEBUG=-O6
# For the time being, the rest is common for all supported SCO flavors
#CPPFLAGS = -DSYSV -DSYSV3
#LDFLAGS=-s
#INDOT=
#LEX=lex
#LIBS=-ll -lnsl_s -lsocket -lc_s
#DESTBIN=/usr/bin
#DESTSBIN = /etc
#DESTEXEC=/etc
#DESTHELP=/usr/lib
#DESTMAN=/usr/local/man
#PIDDIR = /etc
#LDS=:
#RANLIB=:
#COMPINCL=compat/include
#CATEXT = $$$$N
#INSTALL_COMPAT = install-compat
#INSTALL = scoinst
#MANROFF= (tbl | groff -man -Tascii)
#PS = ps -p
#IOT = IOT

#(UNIXWARE)
#CC = cc $(CPPFLAGS)
#CPPFLAGS = -DSVR4 -DBSD_COMP -DUSE_POSIX -D_SYS_STREAM_H
#CDEBUG = -O
#LEX = lex
#INDOT = in.
#XFER_INDOT =
#INSTALL = /usr/ucb/install
# (1.X)
#LIBS = -ll -lsocket -lnsl
# (2.X)
#LIBS = -ll -lsocket -lnsl -lgen
#DESTSBIN = /usr/sbin
#DESTEXEC = /usr/sbin
#ARPREF = `lorder
#ARSUFF = | tsort`
#CATEXT = $$$$N
#PS = ps -p
#PIDDIR = /etc/inet
#INSTALL_COMPAT = install-compat
#RANLIB = :
#LDS = :
#DESTHELP=/usr/lib
#IOT = IOT

#(NeXTstep 2.0ff, single architecture make)
#CC = cc $(CPPFLAGS) #you can use -O2 for 3.2ff
#CPPFLAGS = -Dpid_t=int
#CDEBUG = -g
#LIBS = -ll
#LEX = lex
#DESTEXEC = /usr/etc
#DESTHELP = /usr/lib
#DESTMAN = /usr/man
#DESTSBIN = /usr/etc
#PIDDIR = /etc
#MANDIR = man
#CATEXT = $$$$N
#MANROFF = cat
#INSTALL_COMPAT = install-compat
#IOT = IOT

#(NeXTstep 3.3 Multi Architecture Binaries)
#CC = cc $(CPPFLAGS) -arch i386 -arch m68k -arch hppa -arch sparc
#CDEBUG = -O2
#CPPFLAGS = -Dpid_t=int
#LDS = :
#AR = libtool -o
#RANLIB = :
#LIBS = -ll
#LEX = lex
#DESTEXEC = /usr/etc
#DESTHELP = /usr/lib
#DESTMAN = /usr/man
#DESTSBIN = /usr/etc
#PIDDIR = /etc
#MANDIR = man
#CATEXT = $$$$N
#MANROFF = cat
#INSTALL_COMPAT = install-compat
#IOT = IOT

#(Pyramid dcosx)
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSVR4 -DPOSIX_SIGNALS
#LEX = lex
#LIBS = -ll -lnsl -lsocket
#RANLIB = :
#PS = ps -p
#IOT = IOT

#(Sequent Dynix/PTX)
#CC = cc $(CPPFLAGS) -Wc,-pw
#CDEBUG = -g
#CPPFLAGS = -Du_int=\"unsigned int\" -DSYSV
#RANLIB = :
#LIBS = -ll -lsocket -linet -lnsl -lseq
#PIDDIR = /etc
#DESTSBIN = /usr/local/etc
#DESTEXEC = /usr/local/etc
#DESTHELP = /usr/local/lib
#LEX = lex
#PS = ps -p
#IOT = IOT

#(dgux)
#LEX = lex
#RANLIB =
#CC = gcc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -D__STDC__ -DDGUX
#LIBS = -ll
#DESTBIN = /usr/bin
#DESTSBIN = /usr/bin
#DESTEXEC = /usr/bin
#DESTMAN = /usr/local/man
#DESTHELP = /usr/lib
#DESTLIB = /usr/lib
#DESTINC = /usr/include
#PIDDIR = /etc
#PS = ps -p
#IOT = IOT

#(epix) Has to be compiled and loaded with -systype bsd43
# Epix 2.x.x requires /bsd43/bin in front of the PATH variable
#CC = cc $(CPPFLAGS) -signed -systype bsd43
#CDEBUG = -g
#CPPFLAGS = -DSTDIN_FILENO=0
#LEX = lex
#LIBS = -ll
#DESTSBIN = /etc/local
#DESTEXEC = /etc/local
#DESTMAN = /usr/local/man
#DESTHELP = /usr/lib
#INSTALL = install.sh
#RANLIB = :
#PS = ps -p
#IOT = IOT

#Mips RISC/os 4.52
#CC = cc $(CPPFLAGS) -systype bsd43
#CPPFLAGS = -Dpid_t=int
#CDEBUG = -O -g3 -Olimit 1000
#LEX = lex
#LIBS = -ll
#PIDDIR = /etc
#DESTBIN = /usr/local/bin
#DESTSBIN = /usr/etc
#DESTEXEC = /usr/etc
#DESTMAN = /usr/local/man
#DESTHELP = /usr/local/lib
#DESTLIB = /usr/local/lib
#DESTINC = /usr/local/include
#PS = ps -p
#IOT = IOT

#Esix SVR4.0.4 with gcc
# also works for Dell 2.2 and other i386 SVR4.0. Probably Unixware 1.1 as well.
#Be careful with installation - note in particular that the system ships
#with /usr/sbin/in.named (with in.) and /usr/sbin/named-xfer (without in.)
#to emulate this naming, INDOT should be set to in. and XFER_INDOT should be
#set to empty. also, use 'make install' at your own risk.
#don't include sys/stream.h via netinet/in.h by defining _SYS_STREAM_H.
#CC = gcc $(CPPFLAGS)
#CPPFLAGS = -DSVR4 -DBSD_COMP -DUSE_POSIX -D_SYS_STREAM_H
#CDEBUG = -O
#LEX = lex
#INDOT = in.
#XFER_INDOT =
#PIDDIR = /etc
#INSTALL = /usr/ucb/install
#LIBS = -ll -lsocket -lnsl
#DESTSBIN = /usr/sbin
#DESTEXEC = /usr/sbin
#LDS = @:
#RANLIB = @:
#ARPREF = `lorder
#ARSUFF = | tsort`
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT

#(ISC4.0 using GCC)
#CC = gcc $(CPPFLAGS) -posix
#CPPFLAGS = -DISC
#CDEBUG = -g
#LEX = flex -I
#LIBS = -lbsd
#PIDDIR = /etc
#DESTBIN = /usr/bin
#DESTSBIN = /etc
#DESTEXEC = /etc
#DESTHELP = /etc
#DESTMAN = /usr/catman/l_man
#CATEXT = $$$$N
#RANLIB = @:
#LDS = @:
#PS = ps -p
#ARPREF = `lorder
#ARSUFF = | tsort`
#IOT = IOT

# AUX 3.x (I used 3.1.1)
#CC = gcc $(CPPFLAGS)
#CDEBUG = -O2
#CPPFLAGS = -D_POSIX_SOURCE -DSYSV
#LEX = lex
#LIBS = -ll -lposix -lmalloc -s
#PIDDIR = /etc
#DESTBIN = /etc
#DESTSBIN = /etc
#DESTEXEC = /etc
#DESTMAN = /usr/local/man
#DESTHELP = /etc
#COMPINCL = compat/include
#CATEXT = $$$$N
#INSTALL_COMPAT = install-compat
#INSTALL = install
#RANLIB = @:
#PS = ps -p
#IOT = IOT

#NCR System V/MLS  
#use same definitions as for sunos4.x except for the following
#be careful with installation - note in particular that the system ships
#with /usr/sbin/in.named (with in.) and /usr/sbin/named-xfer (without in.)
#to emulate this naming, INDOT should be set to 'in.' while XFER_INDOT 
#should be empty. also, under solaris2.x, use 'make install' at your own risk.
#don't include sys/stream.h via netinet/in.h by defining _SYS_STREAM_H.
#PIDDIR = /usr/local/lib
#LEX = lex
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSVR4 -D_SYS_STREAM_H -Hnocopyr -DPOSIX_SIGNALS -DNCR -w
#INDOT = in.
#XFER_INDOT =
#LIBS = -ll -lnsl -lsocket
#DESTEXEC = /usr/local/bin
#LDS = :
#RANLIB = :
#ARPREF = `lorder
#ARSUFF = | tsort`
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT

#NCR System Vr4.3 3.x
#also, use 'make install' at your own risk.
#PIDDIR = /etc/inet
#LEX = lex
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSVR4 -Hnocopyr -DPOSIX_SIGNALS -DNO_SA_LEN -DNCR -w
#INDOT = in.
#XFER_INDOT = in.
#LIBS = -ll -lnsl -lsocket
#DESTEXEC = /usr/sbin
#LDS = :
#RANLIB = :
#ARPREF = `lorder
#ARSUFF = | tsort`
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT

#NCR System Vr4.3-3.0
#also, use 'make install' at your own risk.
#PIDDIR = /etc/inet
#LEX = lex
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSVR4 -Hnocopyr -DPOSIX_SIGNALS -DNO_SA_LEN -DNCR -w
#INDOT = in.
#XFER_INDOT = in.
#LIBS = -ll -lnsl -lsocket
#DESTEXEC = /usr/sbin
#LDS = :
#RANLIB = :
#ARPREF = `lorder
#ARSUFF = | tsort`
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT
#SHCC = cc $(CPPPFLAGS)
#PIC = -Kpic
#SHLD = cc -G
#SHRES = shres/ncr

#NCR System Vr4.3-2.03
#also, use 'make install' at your own risk.
#PIDDIR = /etc
#LEX = lex
#CC = cc $(CPPFLAGS)
#CDEBUG = -g
#CPPFLAGS = -DSVR4 -Hnocopyr -DPOSIX_SIGNALS -DNO_SA_LEN -DNCR -w
#INDOT =
#XFER_INDOT =
#LIBS = -ll -lnsl -lsocket
#DESTSBIN = /usr/etc
#DESTEXEC = /usr/etc
#LDS = :
#RANLIB = :
#ARPREF = `lorder
#ARSUFF = | tsort`
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT
#SHCC = cc $(CPPPFLAGS)
#PIC = -Kpic
#SHLD = cc -G
#SHRES = shres/ncr

#(UXP/DS)
#PIDDIR = /etc
#LEX = lex
#CC = cc $(CPPFLAGS)
#CDEBUG = -O
#CPPFLAGS = -DSVR4 -D_SYS_STREAM_H -DUSE_POSIX
#INDOT = in.
#XFER_INDOT =
#LIBS = -ll -lnsl -lsocket
#LDS = :
#COMPINCL = compat/include
#AR = ar -cru
## for V10L20
#INSTALL = /usr/ucb/install -g sys
## for V20L10
#INSTALL = /usr/ucb/install
#RANLIB = :
#ARPREF = `lorder
#ARSUFF = | tsort`
#CATEXT = $$$$N
#PS = ps -p
#IOT = IOT
#PIC = -Kpic
#MANROFF = ( tbl | nroff -man )
#MANDIR = man

#(MOTOROLA Rel FH40.4x ,m88k)
#Support for Motorola m88k machines. The compiler supports
#three compile flags for processor type
#   -F88000 the generic 88k
#   -F88100 for the 88100 chip
#   -F88110 for the 88110 chip
#
# Set to which version of CPU is being used. This was
# compiled on a 197DP with the -F88110 flag
# Earlier boards (187 and V/88) need 88100 and 88000
#
# See /usr/ccs/lib/.compilerc
#
#CC = /usr/ccs/bin/cc $(CPPFLAGS)
#CDEBUG = -g -O
#CPPFLAGS = -F88110 -D_SYS_STREAM_H -DSVR4
#INDOT -in.
#XFER_INDOT =
#LEX = lex
#LIBS = -lsocket -lnsl -ll
#DESTBIN = /usr/bin
#DESTSBIN = /usr/sbin
#DESTEXEC = /etc
#DESTHELP = /usr/lib
#DESTLIB = /usr/lib
#DESTINC = /usr/include
#DESTMAN = /usr/share/man
#PIDDIR = /etc
#LDS = @:
#LDFLAGS=-s
#RANLIB = touch
#COMPINCL = compat/include
#CATEXT = $$$$N
#INSTALL_COMPAT = install-compat
#INSTALL = /usr/ucb/install
#PS =ps -p
#IOT = IOT

####################################################################
############ no user servicable parts beyond this point ############
####################################################################

MANARGS = \
	"SHELL=${SHELL}" "INDOT=${INDOT}" "XFER_INDOT=${XFER_INDOT}" \
	"DESTDIR=${DESTDIR}" "DESTMAN=${DESTMAN}" "INSTALL=${INSTALL}" \
	CATEXT='${CATEXT}' MANDIR='${MANDIR}' MANROFF='${MANROFF}'

MARGS =	"VER=${VER}" "CC=${CC}" "CDEBUG=${CDEBUG}" "LIBS=${LIBS}" \
	"INCL=../${INCL}" "RES=../${RES}" "LEX=${LEX}" "LDFLAGS=${LDFLAGS}" \
	"PIDDIR=${PIDDIR}" "DESTBIN=${DESTBIN}" "DESTSBIN=${DESTSBIN}" \
	"DESTEXEC=${DESTEXEC}" "COMPINCL=../${COMPINCL}" \
	"COMPLIB=../${COMPLIB}" "DESTLIB=${DESTLIB}" "DESTINC=${DESTINC}" \
	"DESTHELP=${DESTHELP}" "PIC=${PIC}" "SHCC=${SHCC}" "SHLD=${SHLD}" \
	"AR=${AR}" "RANLIB=${RANLIB}" "LDS=${LDS}" 'ARPREF=${ARPREF}' \
	'ARSUFF=${ARSUFF}' $(MANARGS) PS="${PS}" "IOT=${IOT}" \
	"CPPFLAGS=${CPPFLAGS}" "KSYMS=${KSYMS}" "KMEM=${KMEM}" \
	"UDPSUM=${UDPSUM}" "SHVER=${SHVER}" "SHMAJVER=${SHMAJVER}"

MACHINE = native
DST = $(MACHINE).b
SRC = ..

SUBDIRS = res $(SHRES) include compat named tools man

all:: FRC

all clean depend:: FRC
	@for x in $(SUBDIRS); do \
		(cd $$x; pwd; $(MAKE) $(MARGS) $@); \
	done

$(SUBDIRS):: FRC
	@for x in $@; do \
		(cd $$x; pwd; $(MAKE) $(MARGS) all); \
	done

clean:: FRC
	-test -d doc/bog && (cd doc/bog; pwd; $(MAKE) $(MARGS) $@)
	(cd conf; rm -f *~ *.CKP *.BAK *.orig)
	rm -f *~ *.CKP *.BAK *.orig

depend:: FRC

mkdirs: FRC
	-set +e; for x in $(DESTDIR)$(DESTBIN) $(DESTDIR)$(DESTSBIN) \
		  $(DESTDIR)$(DESTEXEC) $(DESTDIR)$(DESTMAN) \
		  $(DESTDIR)$(DESTHELP) $(DESTDIR)$(DESTLIB) \
		  $(DESTDIR)$(DESTINC); do \
		test -d $$x || mkdir -p $$x; \
	done

install: FRC
	@for x in $(SUBDIRS); do \
		y=`if [ "X$$x" = "Xcompat" ]; \
			then echo ${INSTALL_COMPAT}; \
			else echo install; \
		   fi`; \
		(cd $$x; pwd; $(MAKE) $(MARGS) $$y); \
	done

D = OPTIONS master bin doc named CHANGES README compat include res \
	Makefile TODO conf man tools 

copyright: FRC
	bin/copyright <conf/copyright `find ${D} -type f ! -name '*,v' -print`

tar: FRC
	( cd doc/bog; make file.psf file.lst )
	find * '(' -name RCS -o -name obj -o -name .depend ')' \
		-print >/tmp/bindXf
	gnutar -c -z -f ../bind-${VER}.tar.gz           -X /tmp/bindXf .
	rm -f /tmp/bindXf

links: $(DST)

$(DST): FRC
	@set -ex; mkdir $(DST); cd $(DST); \
	ln -s $(SRC) SRC; \
	cp SRC/Makefile .; chmod +w Makefile; \
	( mkdir include; cd include; ln -s ../SRC/include SRC; \
	  cp SRC/Makefile .; chmod +w Makefile; \
	  ln -s SRC/*.h SRC/arpa .; \
	); \
	( mkdir conf; cd conf; ln -s ../SRC/conf SRC; \
	  ln -s SRC/*.h .; \
	); \
	( mkdir tools; cd tools; ln -s ../SRC/tools SRC; \
	  ln -s SRC/*.[ch] .; \
	  cp SRC/Makefile .; chmod +w Makefile; \
	  ( mkdir nslookup; cd nslookup; ln -s ../SRC/nslookup SRC; \
	    cp SRC/Makefile .; chmod +w Makefile; \
	    ln -s SRC/[a-z]* .; \
	  ); \
	); \
	( mkdir res; cd res; ln -s ../SRC/res SRC; \
	  cp SRC/Makefile .; chmod +w Makefile; \
	  ln -s SRC/*.[ch] .; \
	); \
	( mkdir compat; cd compat; ln -s ../SRC/compat SRC; \
	  cp SRC/Makefile .; chmod +w Makefile; \
	  ln -s SRC/include .; \
	  ( mkdir lib; cd lib; ln -s ../SRC/lib SRC; \
	    cp SRC/Makefile .; chmod +w Makefile; \
	    ln -s SRC/*.c .; \
	  ); \
	); \
	( mkdir named; cd named; ln -s ../SRC/named SRC; \
	  cp SRC/Makefile .; chmod +w Makefile; \
	  ln -s SRC/*.[ch] SRC/*.sh .; \
	); \
	( mkdir shres; cd shres; ln -s ../SRC/shres SRC; \
	  ( mkdir sunos; cd sunos; ln -s ../../SRC/shres/sunos SRC; \
	    cp SRC/Makefile .; chmod +w Makefile; \
	    ln -s SRC/makeshlib SRC/*.patch* .; \
	  ); \
	  ( mkdir netbsd; cd netbsd; ln -s ../../SRC/shres/netbsd SRC; \
	    cp SRC/Makefile .; chmod +w Makefile; \
	    ln -s SRC/makeshlib SRC/lorder-sparc.sed SRC/shlibname.awk .; \
	  ); \
	  ( mkdir solaris; cd solaris; ln -s ../../SRC/shres/solaris SRC; \
	    cp SRC/Makefile .; chmod +w Makefile; \
	  ); \
	  ( mkdir irix; cd irix; ln -s ../../SRC/shres/irix SRC; \
	    cp SRC/Makefile .; chmod +w Makefile; \
	  ); \
	  ( mkdir linux; cd linux; ln -s ../../SRC/shres/linux SRC; \
	    cp SRC/Makefile .; chmod +w Makefile; \
	  ); \
	); \
	( mkdir man; cd man; ln -s ../SRC/man SRC; \
	  cp SRC/Makefile .; chmod +w Makefile; \
	  ln -s SRC/*.[0-9] .; \
	)

gcc2-lint: FRC
	@-make CC="gcc2 -Wall -Wno-comment -Wno-parentheses -Dlint" all \
		2>&1 | grep '\.[ch]:[0-9]'

FRC:
