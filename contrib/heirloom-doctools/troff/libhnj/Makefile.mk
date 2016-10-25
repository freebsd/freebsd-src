FLAGS =

.c.o:
	$(CC) $(_CFLAGS) $(FLAGS) -c $<

OBJ = hnjalloc.o hyphen.o

all: libhnj.a test

libhnj.a: $(OBJ)
	$(AR) crs $@ $(OBJ)

test: test.o libhnj.a
	$(CC) $(_CFLAGS) $(_LDFLAGS) test.o -L. -lhnj -o test

install:

clean:
	rm -f $(OBJ) test test.o core log *~

mrproper: clean
	rm -f libhnj.a
