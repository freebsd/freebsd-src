#
# (C)opyright 1993-1996 by Darren Reed.
#
# This code may be freely distributed as long as it retains this notice
# and is not changed in any way.  The author accepts no responsibility
# for the use of this software.  I hate legaleese, don't you ?
#
OBJS=ipsend.o ip.o ipsopt.o
ROBJS=ipresend.o ip.o resend.o
TOBJS=iptest.o iptests.o ip.o
BPF=sbpf.o
NIT=snit.o
SUNOS4=sock.o arp.o
BSD=sock.o 44arp.o
LINUX=lsock.o slinux.o larp.o
LINUXK=
TOP=..
SUNOS5=dlcommon.o sdlpi.o arp.o

CC=gcc
CFLAGS=-g -I.. -DNO_IPF

all:
	@echo "Use one of these targets:"
	@echo "	sunos4-nit (standard SunOS 4.1.x)"
	@echo "	sunos4-bpf (SunOS4.1.x with BPF in the kernel)"
	@echo "	bsd-bpf (4.4BSD variant with BPF in the kernel)"
	@echo "	linux10 (Linux 1.0 kernels)"
	@echo "	linux20 (Linux 2.0 kernels)"
	@echo "	sunos5 (Solaris 2.x)"

.c.o:
	$(CC) $(CFLAGS) $(LINUXK) -c $< -o $@

install:
	-$(INSTALL) -cs -g wheel -m 755 -o root ipsend ipresend iptest $(BINDEST)

bpf sunos4-bpf :
	make ipsend "OBJS=$(OBJS)" "UNIXOBJS=$(BPF) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"
	make ipresend "ROBJS=$(ROBJS)" "UNIXOBJS=$(BPF) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"
	make iptest "TOBJS=$(TOBJS)" "UNIXOBJS=$(BPF) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"

nit sunos4 sunos4-nit :
	make ipsend "OBJS=$(OBJS)" "UNIXOBJS=$(NIT) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"
	make ipresend "ROBJS=$(ROBJS)" "UNIXOBJS=$(NIT) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"
	make iptest "TOBJS=$(TOBJS)" "UNIXOBJS=$(NIT) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"

dlpi sunos5 :
	make ipsend "OBJS=$(OBJS)" "UNIXOBJS=$(SUNOS5)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -Dsolaris" "LIBS=-lsocket -lnsl"
	make ipresend "ROBJS=$(ROBJS)" "UNIXOBJS=$(SUNOS5)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -Dsolaris" "LIBS=-lsocket -lnsl"
	make iptest "TOBJS=$(TOBJS)" "UNIXOBJS=$(SUNOS5)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -Dsolaris" "LIBS=-lsocket -lnsl"

bsd-bpf :
	make ipsend "OBJS=$(OBJS)" "UNIXOBJS=$(BPF) $(BSD)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"
	make ipresend "ROBJS=$(ROBJS)" "UNIXOBJS=$(BPF) $(BSD)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"
	make iptest "TOBJS=$(TOBJS)" "UNIXOBJS=$(BPF) $(BSD)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS) -DDOSOCKET"

linuxrev :
	make ipsend "OBJS=$(OBJS)" "UNIXOBJS=$(LINUX)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -I/usr/src/linux/include -DDOSOCKET" $(LINUXK)
	make ipresend "ROBJS=$(ROBJS)" "UNIXOBJS=$(LINUX)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -I/usr/src/linux/include -DDOSOCKET" $(LINUXK)
	make iptest "TOBJS=$(TOBJS)" "UNIXOBJS=$(LINUX)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -I/usr/src/linux/include -DDOSOCKET" $(LINUXK)

linux10:
	make linuxrev 'LINUXK="LINUXK=-DLINUX=0100"'

linux20:
	make linuxrev 'LINUXK="LINUXK=-DLINUX=0200"'

ipsend: $(OBJS) $(UNIXOBJS)
	$(CC) $(OBJS) $(UNIXOBJS) -o $@ $(LIBS)

ipresend: $(ROBJS) $(UNIXOBJS)
	$(CC) $(ROBJS) $(UNIXOBJS) -o $@ $(LIBS)

iptest: $(TOBJS) $(UNIXOBJS)
	$(CC) $(TOBJS) $(UNIXOBJS) -o $@ $(LIBS)

clean:
	rm -rf *.o *core a.out ipsend ipresend iptest
