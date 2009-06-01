 

PROG=	acpixtract
SRCS=	acpixtract.c

CFLAGS+= -Wall -O2 -D_LINUX -DACPI_APPLICATION -Wstrict-prototypes -I../../include 


acpixtract : $(patsubst %.c,%.o, $(SRCS))
	$(CC) $(LDFLAGS) $(patsubst %.c,%.o, $(SRCS)) -o $(PROG)

CLEANFILES= $(PROG)

clean : 
	rm -f $(CLEANFILES) $(patsubst %.c,%.o, $(SRCS))

