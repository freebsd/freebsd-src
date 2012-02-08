
#
# Adjust the following to control which options minitar gets
# built with.  See comments in minitar.c for details.
#
CFLAGS=				\
	-DNO_BZIP2_CREATE	\
	-I../../libarchive	\
	-g

# How to link against libarchive.
LIBARCHIVE=	../../libarchive/libarchive.a

all: minitar

minitar: minitar.o
	cc -g -o minitar minitar.o $(LIBARCHIVE) -lz -lbz2
	strip minitar
	ls -l minitar

minitar.o: minitar.c

clean::
	rm -f *.o
	rm -f minitar
	rm -f *~
