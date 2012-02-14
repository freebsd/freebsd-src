UWXINCDIR = ../include
UWXLIBDIR = ../lib/ipf32

CFLAGS = -O -I $(UWXINCDIR)
LDFLAGS = -L $(UWXLIBDIR)

dumpmyself:	dumpmyself.o dump_context.o primeregs.o
	$(CC) -o dumpmyself $(LDFLAGS) dumpmyself.o dump_context.o primeregs.o -luwx


dumpmyself.o:	$(UWXINCDIR)/uwx.h $(UWXINCDIR)/uwx_self.h

dump_context.o:	
primeregs.o:
