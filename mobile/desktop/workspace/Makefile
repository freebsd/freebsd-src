CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = workspace.o

all: libworkspace.a

libworkspace.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

workspace.o: workspace.c workspace.h
	$(CC) $(CFLAGS) -c workspace.c -o workspace.o

clean:
	rm -f $(OBJS) libworkspace.a

.PHONY: all clean
