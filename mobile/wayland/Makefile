# Makefile for UOS Wayland Compositor
# BSD-licensed

CC ?= cc
CFLAGS ?= -Wall -Wextra -O2 -fPIC
CPPFLAGS ?= -I. -I../gfx -I../input
LDFLAGS ?= -L../gfx

# Libraries
LIBS = -ldrm -ludev -linput -lEGL -lGLESv2 -lm

# Source files
SRCS = compositor.c backend.c renderer.c seat.c xdg-shell.c window.c shell.c output.c
OBJS = $(SRCS:.c=.o)

# Library
LIB = libuos-wayland.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB)

# Install
PREFIX ?= /usr/local
install: $(LIB)
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include/uos
	install -m 644 $(LIB) $(PREFIX)/lib/
	install -m 644 *.h $(PREFIX)/include/uos/

# Test build
test: $(LIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o test-compositor test.c $(LIB) $(LIBS)