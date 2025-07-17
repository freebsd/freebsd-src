SRCS = \
	diff_atomize_text.c \
	diff_main.c \
	diff_myers.c \
	diff_patience.c \
	diff_output.c \
	diff_output_plain.c \
	diff_output_unidiff.c \
	diff_output_edscript.c \
	$(END)

# Compat sources
VPATH=         $(CURDIR)/../compat
SRCS+=         getprogname_linux.c reallocarray.c recallocarray.c merge.c \
	       strlcat.c
CFLAGS+=       -I$(CURDIR)/../compat/include

OBJS = $(SRCS:.c=.o)

libdiff.a: $(OBJS)
	ar rcs $@ $^

CFLAGS += -fsanitize=address -fsanitize=undefined -g -O3
CFLAGS += -Wstrict-prototypes -Wunused-variable -Wuninitialized

%.o: %.c ./*.h ../include/*.h
	gcc $(CFLAGS) -I../include -o $@ -c $<

.PHONY: clean
clean:
	-rm $(OBJS)
	-rm libdiff.a
