# Makefile for mined

CFLAGS	= -D_POSIX_SOURCE -DASSUME_CONS25

OBJ = mined1.o mined2.o

all:	mined

mined:	$(OBJ)
	$(CC) -static -o $@ $(OBJ)

install:	/usr/bin/mined


$(OBJ):	mined.h

clean:
	rm -f mined *.o *.s core *.bak
