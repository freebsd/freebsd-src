CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = task_switcher.o

all: libtask_switcher.a

libtask_switcher.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

task_switcher.o: task_switcher.c task_switcher.h
	$(CC) $(CFLAGS) -c task_switcher.c -o task_switcher.o

clean:
	rm -f $(OBJS) libtask_switcher.a

.PHONY: all clean
