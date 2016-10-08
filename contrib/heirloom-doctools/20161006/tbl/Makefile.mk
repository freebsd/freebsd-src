OBJ = t0.o t1.o t2.o t3.o t4.o t5.o t6.o t7.o t8.o t9.o tb.o tc.o te.o \
	tf.o tg.o ti.o tm.o ts.o tt.o tu.o tv.o version.o

FLAGS = -DMACDIR='"$(MACDIR)"'

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(FLAGS) $(CPPFLAGS) -c $<

all: tbl

tbl: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o tbl

install:
	$(INSTALL) -c tbl $(ROOT)$(BINDIR)/tbl
	$(STRIP) $(ROOT)$(BINDIR)/tbl
	$(INSTALL) -c -m 644 tbl.1 $(ROOT)$(MANDIR)/man1/tbl.1

clean:
	rm -f $(OBJ) tbl core log *~

mrproper: clean

t..o: t..c
t0.o: t0.c t..c
t1.o: t1.c t..c
t2.o: t2.c t..c
t3.o: t3.c t..c
t4.o: t4.c t..c
t5.o: t5.c t..c
t6.o: t6.c t..c
t7.o: t7.c t..c
t8.o: t8.c t..c
t9.o: t9.c t..c
tb.o: tb.c t..c
tc.o: tc.c t..c
te.o: te.c t..c
tf.o: tf.c t..c
tg.o: tg.c t..c
ti.o: ti.c t..c
tm.o: tm.c t..c
ts.o: ts.c
tt.o: tt.c t..c
tu.o: tu.c t..c
tv.o: tv.c t..c
