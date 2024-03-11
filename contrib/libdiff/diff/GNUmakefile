CFLAGS = -fsanitize=address -fsanitize=undefined -g -O3
CFLAGS += -Wstrict-prototypes -Wunused-variable -Wuninitialized

SRCS=	diff.c
LIB=	../lib/libdiff.a

# Compat sources
CFLAGS+=       -I$(CURDIR)/../compat/include

diff: $(SRCS) $(LIB)
	gcc $(CFLAGS) -I../include -o $@ $^

../lib/libdiff.a: ../lib/*.[hc] ../include/*.h
	$(MAKE) -C ../lib

.PHONY: clean
clean:
	rm diff
	$(MAKE) -C ../lib clean
