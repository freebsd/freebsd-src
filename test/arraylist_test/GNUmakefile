.PHONY: regress clean

CFLAGS = -fsanitize=address -fsanitize=undefined -g -O3
CFLAGS += -Wstrict-prototypes -Wunused-variable -Wuninitialized

CFLAGS+=       -I$(CURDIR)/../../compat/include \
	       -I$(CURDIR)/../../include \
	       -I$(CURDIR)/../../lib

$(CURDIR)/arraylist_test: $(CURDIR)/../arraylist_test.c $(CURDIR)/../../lib/libdiff.a
	gcc $(CFLAGS) -o $@ $^

$(CURDIR)/../../lib/libdiff.a: $(CURDIR)/../../lib/*.[hc] $(CURDIR)/../../include/*.h
	$(MAKE) -C $(CURDIR)/../../lib

regress: $(CURDIR)/arraylist_test
	$(CURDIR)/arraylist_test

clean:
	-rm $(CURDIR)/arraylist_test
