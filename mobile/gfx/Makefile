# Makefile for mobile/gfx

CC = cc
CFLAGS = -Wall -Wextra -O2 -I. -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LIBS = -ldrm -lgbm -lEGL -lGLESv2

SRC = drm.c gbm.c egl.c gl.c cursor.c gpu_detect.c renderer.c
OBJ = $(SRC:.c=.o)
TARGET = libgfx.a

all: $(TARGET)

$(TARGET): $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean