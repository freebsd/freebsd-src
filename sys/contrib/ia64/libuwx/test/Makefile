UWXINCDIR = ../include
UWXLIBDIR = ../lib/ipf32

CFLAGS = -O -I $(UWXINCDIR)
LDFLAGS = -L $(UWXLIBDIR)

dumpmyself:	dumpmyself.o dump_context.o
	$(CC) -o dumpmyself $(LDFLAGS) dumpmyself.o dump_context.o -luwx


dumpmyself.o:	$(UWXINCDIR)/uwx.h $(UWXINCDIR)/uwx_self.h

dump_context.o:	
