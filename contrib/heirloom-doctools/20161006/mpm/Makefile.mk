OBJ = misc.o page.o queue.o range.o slug.o version.o

FLAGS = $(EUC) $(DEFINES)

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(FLAGS) $(CPPFLAGS) -c $<

.cc.o:
	$(CXX) $(CFLAGS) $(WARN) $(FLAGS) $(CPPFLAGS) -c $<

all: pm

pm: $(OBJ)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -lm -o pm

install: all
	test -d $(ROOT)$(LIBDIR) || mkdir -p $(ROOT)$(LIBDIR)
	$(INSTALL) -c pm $(ROOT)$(LIBDIR)/pm
	$(STRIP) $(ROOT)$(LIBDIR)/pm

clean:
	rm -f $(OBJ) pm core log *~

mrproper: clean

misc.o: misc.cc misc.h
page.o: page.cc misc.h slug.h range.h page.h
queue.o: queue.cc misc.h slug.h range.h page.h
range.o: range.cc misc.h slug.h range.h
slug.o: slug.cc misc.h slug.h
