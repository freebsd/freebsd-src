CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I.
LDFLAGS =
TARGET = libinput.a

SRCS = evdev.c input_backend.c multitouch.c gestures2.c keyboard.c pointing.c input_hal.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean