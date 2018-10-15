CC_VERSION := $(shell $(CC) --version | \
    sed -n -e '/clang-/s/.*clang-\([0-9][0-9]*\).*/\1/p')
ifeq ($(CC_VERSION),)
# probably not clang
CC_VERSION := 0
endif

WFLAGS :=

# Warnings are version-dependent, unfortunately,
# so test for version before adding a -W flag.
# Note: gnu make requires $(shell test ...) for "a > b" type tests.
ifeq ($(shell test $(CC_VERSION) -gt 0; echo $$?),0)
WFLAGS += -Weverything
WFLAGS += -Wno-padded
WFLAGS += -Wno-gnu-zero-variadic-macro-arguments
WFLAGS += -Wno-format-nonliteral
WFLAGS += -Wno-unused-macros
WFLAGS += -Wno-disabled-macro-expansion
WFLAGS += -Werror
endif

ifeq ($(shell test $(CC_VERSION) -gt 600; echo $$?),0)
WFLAGS += -Wno-reserved-id-macro
endif

CFLAGS := $(WFLAGS) \
	-g \
	-O0 \
	-DL9P_DEBUG=L9P_DEBUG
# Note: to turn on debug, use -DL9P_DEBUG=L9P_DEBUG,
# and set env variable LIB9P_LOGGING to stderr or to
# the (preferably full path name of) the debug log file.

LIB_SRCS := \
	pack.c \
	connection.c \
	request.c \
	genacl.c \
	log.c \
	hashtable.c \
	utils.c \
	rfuncs.c \
	threadpool.c \
	sbuf/sbuf.c \
	transport/socket.c \
	backend/fs.c

SERVER_SRCS := \
	example/server.c

BUILD_DIR := build
LIB_OBJS := $(addprefix build/,$(LIB_SRCS:.c=.o))
SERVER_OBJS := $(SERVER_SRCS:.c=.o)
LIB := lib9p.dylib
SERVER := server

all: build $(LIB) $(SERVER)

$(LIB): $(LIB_OBJS)
	cc -dynamiclib $^ -o build/$@

$(SERVER): $(SERVER_OBJS) $(LIB)
	cc $< -o build/$(SERVER) -Lbuild/ -l9p

clean:
	rm -rf build
	rm -f $(SERVER_OBJS)
build:
	mkdir build
	mkdir build/sbuf
	mkdir build/transport
	mkdir build/backend

build/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
