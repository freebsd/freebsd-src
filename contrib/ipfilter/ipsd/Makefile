#
# Copyright (C) 1993-1997 by Darren Reed.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and due credit is given
# to the original author and the contributors.
#
OBJS=ipsd.o
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/share/man
BPF=sbpf.o
NIT=snit.o
SUNOS4=
BSD=
LINUX=slinux.o
SUNOS5=dlcommon.o sdlpi.o

CC=gcc
CFLAGS=-g -I.. -I../ipsend

all:
	@echo "Use one of these targets:"
	@echo "	sunos4-nit (standard SunOS 4.1.x)"
	@echo "	sunos4-bpf (SunOS4.1.x with BPF in the kernel)"
	@echo "	bsd-bpf (4.4BSD variant with BPF in the kernel)"
	@echo "	linux (Linux kernels)"
	@echo "	sunos5 (Solaris 2.x)"

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

ipsdr: ipsdr.o
	$(CC) ipsdr.o -o $@ $(LIBS)

bpf sunos4-bpf :
	make ipsd "OBJS=$(OBJS)" "UNIXOBJS=$(BPF) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS)"

nit sunos4 sunos4-nit :
	make ipsd "OBJS=$(OBJS)" "UNIXOBJS=$(NIT) $(SUNOS4)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS)"

sunos5 :
	make ipsd "OBJS=$(OBJS)" "UNIXOBJS=$(SUNOS5)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -Dsolaris" "LIBS=-lsocket -lnsl"

bsd-bpf :
	make ipsd "OBJS=$(OBJS)" "UNIXOBJS=$(BPF) $(BSD)" "CC=$(CC)" \
		"CFLAGS=$(CFLAGS)"

linux :
	make ipsd "OBJS=$(OBJS)" "UNIXOBJS=$(LINUX)" "CC=$(CC)" \
		CFLAGS="$(CFLAGS) -I /usr/src/linux"

ipsd: $(OBJS) $(UNIXOBJS)
	$(CC) $(OBJS) $(UNIXOBJS) -o $@ $(LIBS)

../ipft_sn.o ../ipft_pc.o:
	(cd ..; make $(@:../%=%))

clean:
	rm -rf *.o core a.out ipsd ipsdr
