CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I.
LDFLAGS = 
TARGET_LIB = libaudio.a
TARGET_SERVER = audiod

SRCS = pipeline.c mixer.c dsp.c alsa_compat.c route.c audio_server.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET_LIB) $(TARGET_SERVER)

$(TARGET_LIB): $(OBJS)
	ar rcs $@ $^

$(TARGET_SERVER): audio_server.o $(TARGET_LIB)
	$(CC) $(CFLAGS) -o $@ audio_server.o -L. -laudio $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET_LIB) $(TARGET_SERVER)

.PHONY: all clean