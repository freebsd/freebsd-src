all: libxml2 hubbub

CC = gcc
CFLAGS = -W -Wall --std=c99

LIBXML2_OBJS = libxml2.o
libxml2: libxml2.c
libxml2: CFLAGS += `pkg-config libxml-2.0 --cflags`
libxml2: $(LIBXML2_OBJS)
	gcc -o libxml2 $(LIBXML2_OBJS) `pkg-config libxml-2.0 --libs`


HUBBUB_OBJS = hubbub.o
hubbub: hubbub.c
hubbub: CFLAGS += `pkg-config --cflags libparserutils libhubbub`
hubbub: $(HUBBUB_OBJS)
	gcc -o hubbub $(HUBBUB_OBJS) `pkg-config --libs libhubbub libparserutils`
