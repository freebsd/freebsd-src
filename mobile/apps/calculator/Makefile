CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app

all: calculator.app

calculator.app: calculator.c
	$(CC) $(CFLAGS) -o $@ calculator.c $(INCLUDES)

clean:
	rm -f calculator.app

.PHONY: all clean
