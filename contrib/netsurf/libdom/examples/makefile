CC := gcc
LD := gcc

CFLAGS := `pkg-config --cflags libdom` `pkg-config --cflags libwapcaplet` -Wall -O0 -g
LDFLAGS := `pkg-config --libs libdom` `pkg-config --libs libwapcaplet`

SRC := dom-structure-dump.c

dom-structure-dump: $(SRC:.c=.o)
	@$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) dom-structure-dump $(SRC:.c=.o)

%.o: %.c
	@$(CC) -c $(CFLAGS) -o $@ $<

