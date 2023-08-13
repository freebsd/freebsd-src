CC ?= gcc
CFLAGS ?= -Wall -O2
LDFLAGS ?=
TERMLIB = -lncurses
srcdir ?= .

all: lesstest lt_screen 

LESSTEST_SRC = display.c env.c lesstest.c parse.c pipeline.c log.c run.c term.c wchar.c
LESSTEST_OBJ = $(patsubst %.c,%.o,$(LESSTEST_SRC))

lesstest: $(LESSTEST_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o lesstest $(LESSTEST_OBJ) $(TERMLIB)

LT_SCREEN_SRC = lt_screen.c unicode.c wchar.c
LT_SCREEN_OBJ = $(patsubst %.c,%.o,$(LT_SCREEN_SRC))
lt_screen: $(LT_SCREEN_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o lt_screen $(LT_SCREEN_OBJ)

HDR = lesstest.h lt_types.h wchar.h
*.o: $(HDR)

echo_distfiles:
	@echo $(patsubst %,$(srcdir)/%,$(LESSTEST_SRC) $(LT_SCREEN_SRC) $(HDR) lt/*.lt runtest maketest Makefile)

clean:
	rm -f lesstest lt_screen *.o
