I	=/isdn/bin

OPTIONS	=-DBSD -O

OBJS	      = ittd

all:		$(OBJS)

ittd:		ittd.c
		cc $(OPTIONS) -o ittd ittd.c

install:	all
		strip ittd
		install ittd $I

clean:
		@rm -f $(OBJS) a.out core.*

