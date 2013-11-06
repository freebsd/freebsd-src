#
# $FreeBSD$
#
# For multiple programs using a single source file each,
# we can just define 'progs' and create custom targets.
PROGS	=	pkt-gen bridge vale-ctl testpcap libnetmap.so

CLEANFILES = $(PROGS) pcap.o nm_util.o
NO_MAN=
CFLAGS += -Werror -Wall -nostdinc -I/usr/include -I../../../sys
CFLAGS += -Wextra

LDFLAGS += -lpthread -lpcap

.include <bsd.prog.mk>
.include <bsd.lib.mk>

all: $(PROGS)

pkt-gen bridge: nm_util.o
	$(CC) $(CFLAGS) -o ${.TARGET} ${.TARGET:=.c} nm_util.o $(LDFLAGS)

testpcap: pcap.c libnetmap.so
	$(CC) $(CFLAGS) -DTEST -L. -lnetmap -o ${.TARGET} pcap.c
	
libnetmap.so:	pcap.c nm_util.c
	$(CC) $(CFLAGS) -fpic -c ${.ALLSRC}
	$(CC) -shared -o ${.TARGET} ${.ALLSRC:.c=.o}
