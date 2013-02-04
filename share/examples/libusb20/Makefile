# $FreeBSD$
TARGETS=	bulk control

all: $(TARGETS)

bulk: bulk.o aux.o
	$(CC) $(CFLAGS) -o bulk bulk.o aux.o -lusb

control: control.o aux.o
	$(CC) $(CFLAGS) -o control control.o aux.o -lusb

clean:
	rm -f $(TARGETS) *.o *~
