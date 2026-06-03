# Connectivity Framework Makefile
# BSD licensed
#
# Compiler and flags
CC ?= cc
CFLAGS += -Wall -Wextra -Werror -O2
CFLAGS += -I. -I../services -I../frameworks/ipc
CFLAGS += -I/usr/include -I/usr/local/include

# Libraries
LDFLAGS += -L/usr/lib -L/usr/local/lib
LDLIBS += -lsocket -lcrypto -lutil

# Source files
SRCS = wifi.c dhcp.c network_mgr.c cellular.c bluetooth_mgr.c vpn.c nfc.c
OBJS = $(SRCS:.c=.o)

# Library target
LIBNAME = libconnectivity.a
LIBRARY = $(LIBNAME)

# Test program
TEST_BIN = connectivity_test

# Default target
all: $(LIBRARY)

# Static library
$(LIBRARY): $(OBJS)
	ar rcs $@ $^

# Object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test binary
$(TEST_BIN): $(LIBRARY) test_main.c
	$(CC) $(CFLAGS) $< $(LIBRARY) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

# Install
PREFIX ?= /usr/local
install: $(LIBRARY)
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include/mobile/connectivity
	install -m 644 $(LIBRARY) $(PREFIX)/lib/
	install -m 644 *.h $(PREFIX)/include/mobile/connectivity/

# Clean
clean:
	rm -f $(OBJS) $(LIBRARY) $(TEST_BIN)

# Lint
lint: $(SRCS)
	clang-tidy $^ -- -I.. -I../services -I../frameworks/ipc

.PHONY: all clean install test lint