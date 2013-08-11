# TR turns on tracing, to the specified file.
TR=	-DTR=\"/dev/ttypa\"
#TR=	-DTR=\"__log\"

# VI is the binary that ip_cl runs.
VI=	-DVI=\"../build.local/nvi\"

DEBUG=	-DDEBUG -g
INC=	-I. -I../build.local -I../include
CFLAGS=	$(DEBUG) $(TR) $(VI) $(INC)

OBJS=	ip_cl.o

LIBS=	-lcurses -ltermcap

ip_cl: ${OBJS}
	${CC} ${OBJS} -o $@ ${LIBS}

clean:
	rm -f ip_cl ${OBJS}
